CC ?= gcc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -O2 -g
LDFLAGS ?= -pthread

all: bank_server bank_client

bank_server: server.o sha256.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

bank_client: client.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c bank.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o bank_server bank_client

test: all
	bash integration_test.sh

.PHONY: all clean test
