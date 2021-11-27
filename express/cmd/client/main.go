// Heavily based on Express evaluation code @ https://github.com/SabaEskandarian/Express
//   - see also: https://www.usenix.org/system/files/sec21-eskandarian.pdf
//   - original source code based on denji/golang-tls

package main

/*
#cgo CFLAGS: -O2 -I/usr/local/include
#cgo LDFLAGS: -lcrypto -lm -L/usr/local/lib
#include "../../pkg/dpf.h"
#include "../../pkg/okvClient.h"
#include "../../pkg/dpf.c"
#include "../../pkg/okvClient.c"
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
	//"sync"
	//"time"
	//"unsafe"
)

// connection message types
const (
	NEW_ROW int = iota
	WRITE
)

func main() {

	log.SetFlags(log.Lshortfile)

	// parse configuration
	var leaderIP string
	var followerIP string
	var dataSize, numThreads int

	flag.StringVar(&leaderIP, "leaderIP", "localhost:4442", "IP:port of primary leader server")
	flag.StringVar(&followerIP, "followerIP", "localhost:4443", "IP:port of secondary follower server")
	flag.IntVar(&dataSize, "dataSize", 1024, "size of each mailbox")
	flag.IntVar(&numThreads, "numThreads", 8, "number of client threads")

	flag.Parse()

	log.Printf("starting client for %v-byte messages...\n\texpecting servers at %v and %v\n",
		dataSize, leaderIP, followerIP)

	conf := &tls.Config{
		InsecureSkipVerify: true,
	}

	C.initializeClient(C.int(numThreads))

	// XXX using deterministic keys for testing
	_, clientSecretKey, err := box.GenerateKey(strings.NewReader(strings.Repeat("c", 10000)))
	if err != nil {
		log.Fatal(err)
	}
	s2PublicKey, _, err := box.GenerateKey(strings.NewReader(strings.Repeat("b", 10000)))
	if err != nil {
		log.Fatal(err)
	}
	auditorPublicKey, _, err := box.GenerateKey(strings.NewReader(strings.Repeat("a", 10000)))
	if err != nil {
		log.Fatal(err)
	}

	// XXX
	_ = clientSecretKey
	_ = s2PublicKey
	_ = auditorPublicKey

	connLeader, err := tls.Dial("tcp", leaderIP, conf)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("connected to leader server\n")

	connFollower, err := tls.Dial("tcp", followerIP, conf)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("connected to follower server\n")

	// Example 1 - add a new mailbox to the database, talking to both servers

	// generate a random mailbox key
	var newRowKey [16]byte
	_, err = rand.Read(newRowKey[:])
	if err != nil {
		log.Fatal(err)
	}

	idx, addr := addRow(connLeader, connFollower, newRowKey, dataSize)
	log.Printf("added new mailbox at index %v, addr %v\n", idx, addr)

	connLeader.Close()
	connFollower.Close()
}

func addRow(connLeader, connFollower net.Conn, newRowKey [16]byte, dataSize int) (int, []byte) {

	// tell servers we're adding a new row
	writeBytesToConn(connLeader, intToByte(NEW_ROW)[0:1])
	writeBytesToConn(connFollower, intToByte(NEW_ROW)[0:1])

	// tell servers we just want to add one entry
	writeBytesToConn(connLeader, intToByte(1))
	writeBytesToConn(connFollower, intToByte(1))

	// send row size
	writeBytesToConn(connLeader, intToByte(dataSize))
	writeBytesToConn(connFollower, intToByte(dataSize))

	// send row key
	writeBytesToConn(connLeader, newRowKey[:])
	writeBytesToConn(connFollower, newRowKey[:])

	mailboxIdx := byteToInt(readBytesFromConn(connLeader, 4))
	mailboxAddr := readBytesFromConn(connLeader, 16)

	return mailboxIdx, mailboxAddr
}

/* Utility functions */

func readBytesFromConn(conn net.Conn, n int) []byte {
	payload := make([]byte, n)
	for count := 0; count < n; {
		nRead, err := conn.Read(payload[count:])
		count += nRead
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
