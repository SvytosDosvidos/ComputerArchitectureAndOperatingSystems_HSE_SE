CC ?= gcc
CFLAGS = -std=c17 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -O2
TARGETS = naval_server naval_ship naval_observer

all: $(TARGETS)

naval_server: server.o common.o
	$(CC) $(CFLAGS) -o $@ $^
naval_ship: ship.o common.o
	$(CC) $(CFLAGS) -o $@ $^
naval_observer: observer.o common.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
