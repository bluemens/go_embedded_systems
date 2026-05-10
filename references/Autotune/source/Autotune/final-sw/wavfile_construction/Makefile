CC  = gcc
CXX = g++

# Compilation options:
# -g for debugging info and -Wall enables all warnings

CFLAGS   = -g -Wall $(INCLUDES)
CXXFLAGS = -g -Wall $(INCLUDES)

# Linking options:
# -g for debugging info

LDFLAGS = -g

test: test.o make_wav.o

test.o: test.c make_wav.h

make_wav.o: make_wav.c make_wav.h

.PHONY: clean
clean:
	rm -f *.o make_wav test

.PHONY: all
all: clean make_wav.o