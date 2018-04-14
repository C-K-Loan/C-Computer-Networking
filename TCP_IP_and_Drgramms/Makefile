ABGABE_PREFIX := t03g05

CFLAGS := -std=gnu99 -Wall -Wextra -Wpedantic
CC     := gcc

SRC_DIRS ?= ./
SRCS := $(shell find . -name "*.c")
TARGET := $(basename $(SRCS))

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $@.c -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) $(ABGABE_PREFIX)_block_2_1.zip $(ABGABE_PREFIX)_block_2_2.zip

block_2_1:
	zip $(ABGABE_PREFIX)_$@.zip ex4.c ex5.c Makefile README

block_2_2:
	zip $(ABGABE_PREFIX)_$@.zip ex6_dgram.c ex6_stream.c ex6.txt ex7.c ex7.txt Makefile README_block_2_2
