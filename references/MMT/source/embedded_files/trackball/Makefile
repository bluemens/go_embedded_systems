CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS = -pthread
TARGET = trackball
OBJS = trackball.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

trackball.o: trackball.c trackball.h
	$(CC) $(CFLAGS) -c trackball.c

clean:
	rm -f $(TARGET) $(OBJS)

