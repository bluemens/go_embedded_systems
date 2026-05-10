CC         = gcc
TARGET-TM  = generate_tilemap
TARGET-TS  = tileset_hex_gen
TS_FILES   = $(filter-out $(TARGET-TS).c, $(wildcard *.c))
TM_FILES   = $(filter-out $(TARGET-TM).c, $(wildcard *.c))
TS_OBJS    = $(patsubst %.c,%.o,$(TS_FILES))
TM_OBJS    = $(patsubst %.c,%.o,$(TM_FILES))
CFLAGS     = -g -Wall -Werror -pedantic-errors
LDFLAGS    =

.PHONY: all clean clean-ts clean-tm
all:
	$(CC) $(CFLAGS) $(TARGET-TS).c -o $(TARGET-TS) $(LDFLAGS)
	$(CC) $(CFLAGS) $(TARGET-TM).c -o $(TARGET-TM) $(LDFLAGS)

ts: 
	$(CC) $(CFLAGS) $(TARGET-TS).c -o $(TARGET-TS) $(LDFLAGS)
%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
tm:
	$(CC) $(CFLAGS) $(TARGET-TM).c -o $(TARGET-TM) $(LDFLAGS)
%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
		rm -f $(TS_OBJS) $(TARGET-TS) $(TARGET-TS).exe
		rm -f $(TM_OBJS) $(TARGET-TM) $(TARGET-TM).exe
clean-ts:
	    rm -f $(TS_OBJS) $(TARGET-TS) $(TARGET-TS).exe
clean-tm:
	    rm -f $(TM_OBJS) $(TARGET-TM) $(TARGET-TM).exe