# Makefile for ls-v1.0.0
CC = gcc
CFLAGS = -Wall -g
SRC = src/lsv1.0.0.c
TARGET = bin/ls

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f bin/ls obj/*.o

.PHONY: all clean
