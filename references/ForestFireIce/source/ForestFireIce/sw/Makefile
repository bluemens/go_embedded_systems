# Top directory path
INCLUDEDIR = include
SRCDIR = src
TESTDIR = test

# All source files
SRCS = $(wildcard $(SRCDIR)/*.c)
TEST_SRCS = $(wildcard $(TESTDIR)/*.c)

# Header file path
INCLUDES = -I$(INCLUDEDIR)

# Target file list (.c → .o)
OBJS = $(SRCS:.c=.o)
TEST_OBJS = $(filter-out $(SRCDIR)/main.o, $(OBJS)) $(TEST_SRCS:.c=.o)

# Final executable file name
TARGET = game
TEST_TARGET = test_joypad

# Compiler and options
CC = gcc
CFLAGS = -Wall -O2 $(INCLUDES)
LDLIBS = -lm  # Add math library link

.PHONY: all clean test

all: $(TARGET)

test: $(TEST_TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(TEST_TARGET): $(TEST_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(TESTDIR)/*.o $(TARGET) $(TEST_TARGET)
