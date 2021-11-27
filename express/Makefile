
CC=gcc-11

all: server client

.PHONY: server
server:
	CC=$(CC) go build ./cmd/server 

.PHONY: client
client:
	CC=$(CC) go build ./cmd/client 

