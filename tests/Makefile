# Simple Makefile for host-based unit tests
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I../src -I.
LIBS = -lcjson

# Source files
SRC_DIR = ../src
TEST_SRCS = test_config_utils.c test_json_utils.c test_runner.c
SRC_SRCS = $(SRC_DIR)/config_utils.c $(SRC_DIR)/json_utils.c

# Object files
TEST_OBJS = $(TEST_SRCS:.c=.o)
SRC_OBJS = $(SRC_SRCS:.c=.o)
ALL_OBJS = $(TEST_OBJS) $(SRC_OBJS)

# Test executable
TARGET = run_tests

.PHONY: all clean test install-deps

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(ALL_OBJS)

install-deps:
	sudo apt-get update
	sudo apt-get install -y libcjson-dev

.DEFAULT_GOAL := test