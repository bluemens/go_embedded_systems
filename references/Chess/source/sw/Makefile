# Makefile for compiling chess.c

CC = gcc
CFLAGS = -std=c11
TARGET = chess
OBJS = chess.o uci.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f *.o $(TARGET)