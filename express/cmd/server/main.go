// Heavily based on Express evaluation code @ https://github.com/SabaEskandarian/Express
//   - see also: https://www.usenix.org/system/files/sec21-eskandarian.pdf

package main

/*
#cgo CFLAGS: -fopenmp -O2 -I/usr/local/include
#cgo LDFLAGS: -lcrypto -lm -fopenmp -L/usr/local/lib
#include "../../pkg/dpf.h"
#include "../../pkg/okv.h"
#include "../../pkg/dpf.c"
#include "../../pkg/okv.c"
*/
import "C"
import (
	"crypto/rand"
	"crypto/tls"
	"flag"
	//	"fmt"
	"golang.org/x/crypto/nacl/box"
	//	"io"
	"log"
	"net"
	//	"os"
	//	"strconv"
	"strings"
	"sync"
	"time"
	"unsafe"
)

// connection message types
const (
	NEW_ROW int = iota
	WRITE
)

func main() {

	log.SetFlags(log.Lshortfile)

	// parse configuration
	var leader bool
	var leaderIP, followerIP string
	var numRows int
	var dataSize int
	var numThreads int

	flag.BoolVar(&leader, "leader", false, "set server as primary (e.g. serverA) that communicates with clients")
	flag.StringVar(&leaderIP, "leaderIP", "localhost:4442", "IP:port of leader server (A)")
	flag.StringVar(&followerIP, "followerIP", "localhost:4443", "IP:port of follower server (B)")
	flag.IntVar(&numRows, "numRows", 0, "number of mailbox rows to initialize")
	flag.IntVar(&dataSize, "dataSize", 1024, "size of each mailbox")
	flag.IntVar(&numThreads, "numThreads", 8, "number of threads to handle writes")

	flag.Parse()

	log.Printf("starting server (leader=%v) at %v with %v %v-byte mailboxes...\n\texpecting other server at %v\n",
		leader, leaderIP, numRows, dataSize, followerIP)

	// init backing table
	C.initializeServer(C.int(numThreads))

	// init TLS server
	cert, err := tls.LoadX509KeyPair("pkg/server.crt", "pkg/server.key")
	if err != nil {
		log.Fatal(err)
	}

	config := &tls.Config{Certificates: []tls.Certificate{cert}}
	port := ":"
	if leader {
		port += strings.Split(leaderIP, ":")[1]
	} else {
		port += strings.Split(followerIP, ":")[1]
	}

	listener, err := tls.Listen("tcp", port, config)
	if err != nil {
		log.Fatal(err)
	}
	defer listener.Close()

	// XXX
	// using a deterministic randomness source so everyone shares a key. not for real use
	clientPublicKey, _, err := box.GenerateKey(strings.NewReader(strings.Repeat("c", 10000)))
	if err != nil {
		log.Fatal(err)
	}

	_, s2SecretKey, err := box.GenerateKey(strings.NewReader(strings.Repeat("b", 10000)))
	if err != nil {
		log.Fatal(err)
	}

	// before we start dealing with client connections, let's set up a number
	// of unused rows to pad out database
	addUnusedRows(numRows, dataSize)

	var dbMutex sync.RWMutex

	// set up a channel of connections waiting to be processed, and let our
	// worker threads deal with them one at a time
	conns := make(chan net.Conn)
	for i := 0; i < numThreads; i++ {
		go connectionHandler(i, conns, leader, leaderIP, followerIP, dbMutex, clientPublicKey, s2SecretKey)
	}

	// main loop: accept network connections and queue them for workers
	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Fatal(err)
		}
		conn.SetDeadline(time.Time{})

		conns <- conn
	}
}

func addUnusedRows(numRows int, dataSize int) {
	for i := 0; i < numRows; i++ {
		var unusedRowKey [16]byte
		_, err := rand.Read(unusedRowKey[:])
		if err != nil {
			log.Fatal(err)
		}

		C.processnewEntry(C.int(dataSize), getPtrToBuffer(unusedRowKey[:], 0))
	}
}

func connectionHandler(threadId int, conns chan net.Conn, leader bool, leaderIP string, followerIP string, dbMutex sync.RWMutex,
	clientPublicKey, s2SecretKey *[32]byte) {

	dbSize := int(C.dbSize)
	db := make([][]byte, dbSize)
	for i := 0; i < dbSize; i++ {
		db[i] = make([]byte, int(C.db[i].dataSize))
	}

	for {
		// pick up a connection to handle
		//log.Printf("[%v] waiting for connection\n", threadId)
		conn := <-conns
		//log.Printf("[%v] handling incoming connection...", threadId)
		connType := byteToInt(readBytesFromConn(conn, 1))

		switch connType {
		case NEW_ROW:
			log.Printf("[%v] NEW_ROW\n", threadId)
			dbMutex.Lock()
			handleNewRow(threadId, conn, leader, leaderIP, followerIP)
			dbMutex.Unlock()

		case WRITE:
			log.Printf("[%v] WRITE\n", threadId)
			dbMutex.RLock()
			newSize := int(C.dbSize)
			if dbSize != newSize { // add new rows if necessary
				for i := 0; i < int(C.dbSize)-dbSize; i++ {
					db = append(db, make([]byte, int(C.db[i].dataSize)))
				}
			}
			dbSize = newSize
			dbMutex.RUnlock()

			handleWrite(threadId, conn, leader, leaderIP, followerIP, dbSize, db, clientPublicKey, s2SecretKey)

		default:
			log.Fatal("got unexpected connection type", connType)
		}

		//log.Printf("[%v] done\n", threadId)
	}
}

func handleNewRow(threadId int, conn net.Conn, leader bool, leaderIP string, followerIP string) {

	numNewRows := byteToInt(readBytesFromConn(conn, 4))
	dataSize := byteToInt(readBytesFromConn(conn, 4))

	for i := 0; i < numNewRows; i++ {
		newRowKey := readBytesFromConn(conn, 16)
		newIndex := int(C.processnewEntry(C.int(dataSize), getPtrToBuffer(newRowKey, 0)))

		// if this server is the leader, send the new index (phy addr) and row
		// id (virt addr) back to client
		if leader {
			writeBytesToConn(conn, intToByte(newIndex))
			writeBytesToConn(conn, C.GoBytes(unsafe.Pointer(C.tempRowId), 16))
		}
	}
}

func handleWrite(threadId int, conn net.Conn, leader bool, leaderIP string, followerIP string, dbSize int, db [][]byte, clientPublicKey, s2SecretKey *[32]byte) {

	vector := make([]byte, dbSize*16)

	dataTransferSize := byteToInt(readBytesFromConn(conn, 4))
	dataSize := byteToInt(readBytesFromConn(conn, 4))

	if leader {

		conf := &tls.Config{
			InsecureSkipVerify: true,
		}

		serverConn, err := tls.Dial("tcp", followerIP, conf)
		if err != nil {
			log.Fatal(err)
		}

		input := readBytesFromConn(conn, dataTransferSize)

		clientInputSize := 24 + dataTransferSize + box.Overhead
		clientInput := readBytesFromConn(conn, clientInputSize)

		var seed [16]byte
		_, err = rand.Read(seed[:])
		if err != nil {
			log.Fatal(err)
		}

		waitForClient := make(chan int)
		waitForServer := make(chan int)

		clientDataSize := 160
		clientDataSizeB := 24 + box.Overhead + 160
		var clientAuditInput []byte
		var clientAuditInputB []byte

		go func() {
			writeBytesToConn(serverConn, intToByte(WRITE)[0:1])

			msg := append(intToByte(dataTransferSize), intToByte(dataSize)...)
			msg = append(msg, seed[:]...)
			msg = append(msg, clientInput...)

			writeBytesToConn(serverConn, msg)

			waitForServer <- 1
		}()

		go func() {
			writeBytesToConn(conn, seed[:])

			clientAuditInput = readBytesFromConn(conn, clientDataSize)
			clientAuditInputB = readBytesFromConn(conn, clientDataSizeB)

			waitForClient <- 1
		}()

		applyDPF(dbSize, db, threadId, input, vector)

		// XXX didn't try to decompose cause it's getting late
		mVal := make([]byte, 16)
		cVal := make([]byte, 16)
		C.serverSetupProof(C.ctx[threadId], getPtrToBuffer(seed[:], 0), C.dbSize, getPtrToBuffer(vector, 0), getPtrToBuffer(mVal, 0), getPtrToBuffer(cVal, 0))

		<-waitForClient

		ansA := make([]byte, 96)
		C.serverComputeQuery(C.ctx[threadId], getPtrToBuffer(seed[:], 0), getPtrToBuffer(mVal, 0), getPtrToBuffer(cVal, 0), getPtrToBuffer(clientAuditInput, 0), getPtrToBuffer(ansA, 0))

		<-waitForServer

		writeBytesToConn(serverConn, clientAuditInputB)
		writeBytesToConn(serverConn, ansA)

		ansB := readBytesFromConn(serverConn, 100)
		auditResp := int(C.serverVerifyProof(getPtrToBuffer(ansA, 0), getPtrToBuffer(ansB, 4)))

		if byteToInt(ansB[:4]) == 0 {
			log.Println("audit failed on server B")
		}

		if auditResp == 0 {
			log.Println("audit failed")
		}

		done := 1
		writeBytesToConn(conn, intToByte(done))

	} else { // follower
		seed := readBytesFromConn(conn, 16)

		clientInputSize := 24 + dataTransferSize + box.Overhead
		clientInput := readBytesFromConn(conn, clientInputSize)

		// unbox query
		var decryptNonce [24]byte
		copy(decryptNonce[:], clientInput[:24])
		decryptedQuery, ok := box.Open(nil, clientInput[24:], &decryptNonce, clientPublicKey, s2SecretKey)
		if !ok {
			log.Fatal("decryption not ok!")
		}

		applyDPF(dbSize, db, threadId, decryptedQuery, vector)

		mVal := make([]byte, 16)
		cVal := make([]byte, 16)
		C.serverSetupProof(C.ctx[threadId], getPtrToBuffer(seed, 0), C.dbSize, getPtrToBuffer(vector, 0), getPtrToBuffer(mVal, 0), getPtrToBuffer(cVal, 0))

		proofBox := readBytesFromConn(conn, 24+160+box.Overhead)
		copy(decryptNonce[:], proofBox[:24])
		proof, ok := box.Open(nil, proofBox[24:], &decryptNonce, clientPublicKey, s2SecretKey)
		if !ok {
			log.Fatal("decryption not ok!")
		}

		ansA := readBytesFromConn(conn, 96)
		ansB := make([]byte, 96)
		C.serverComputeQuery(C.ctx[threadId], getPtrToBuffer(seed, 0), getPtrToBuffer(mVal, 0), getPtrToBuffer(cVal, 0), getPtrToBuffer(proof, 0), getPtrToBuffer(ansB, 0))
		auditResp := int(C.serverVerifyProof(getPtrToBuffer(ansA, 0), getPtrToBuffer(ansB, 0)))

		if auditResp == 0 {
			log.Println("audit failed")
		}

		auditOutputs := append(intToByte(auditResp), ansB...)
		writeBytesToConn(conn, auditOutputs)
	}
}

func applyDPF(dbSize int, db [][]byte, threadId int, query []byte, vector []byte) {

	for i := 0; i < dbSize; i++ {
		ds := int(C.db[i].dataSize)
		dataShare := make([]byte, ds)
		v := C.evalDPF(C.ctx[threadId], getPtrToBuffer(query, 0), C.db[i].rowID, C.int(ds), getPtrToBuffer(dataShare, 0))
		copy(vector[i*16:(i+1)*16], C.GoBytes(unsafe.Pointer(&v), 16))
		for j := 0; j < ds; j++ {
			db[i][j] = db[i][j] ^ dataShare[j]
		}
	}
}

/* Utility functions */

func readBytesFromConn(conn net.Conn, n int) []byte {
	payload := make([]byte, n)
	for count := 0; count < n; {
		nRead, err := conn.Read(payload[count:])
		count += nRead
		//log.Printf("read %v bytes\n", count)
		if err != nil && count != n {
			log.Fatal(err, n, count)
		}
	}

	return payload
}

func writeBytesToConn(conn net.Conn, payload []byte) {
	nWritten, err := conn.Write(payload)
	if err != nil || nWritten != len(payload) {
		log.Fatal(err, nWritten)
	}
}

func getPtrToBuffer(buf []byte, idx int) *C.uchar {
	return (*C.uchar)(&buf[idx])
}

func byteToInt(bytes []byte) int {
	nBytes := len(bytes)
	if nBytes > 4 {
		nBytes = 4
	}

	x := 0
	for i := 0; i < nBytes; i++ {
		x += int(bytes[i]) << (i * 8)
	}
	return x
}

func intToByte(x int) []byte {
	bytes := make([]byte, 4)
	for i := 0; i < 4; i++ {
		bytes[i] = byte((x >> (i * 8)) & 0xff)
	}
	return bytes
}
