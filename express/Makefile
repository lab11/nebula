
CC=gcc

all: server client

.PHONY: server
server:
	CC=$(CC) go build ./cmd/server 

.PHONY: client
client:
	CC=$(CC) go build ./cmd/client 

.PHONY: clean
clean:
	go clean -cache -testcache ./cmd/server
	go clean -cache -testcache ./cmd/client
	rm client server

