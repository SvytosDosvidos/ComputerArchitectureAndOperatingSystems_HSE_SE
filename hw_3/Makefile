CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread -lrt

TARGET = sea_battle
SRCS = main.c judge.c command_center.c ship.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) battle.log
	rm -f /tmp/sb22_fifo_*
