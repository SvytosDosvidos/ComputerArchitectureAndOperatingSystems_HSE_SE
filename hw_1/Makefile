CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -O2 -Iinclude
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/board.c \
       $(SRC_DIR)/ship.c \
       $(SRC_DIR)/fleet.c \
       $(SRC_DIR)/judge.c \
       $(SRC_DIR)/logger.c \
       $(SRC_DIR)/config.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/naval_battle

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) *.log

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	chmod +x tests/run_tests.sh
	./tests/run_tests.sh
