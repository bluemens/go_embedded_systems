ifneq ($(KERNELRELEASE),)

# Kernel obj
obj-m := vga_poker.o

else

KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
PWD           := $(shell pwd)
CC            := gcc
CFLAGS        := -std=c99 -Wall -Wextra

USER_OBJS := poker_logic.o controller.o

.PHONY: all module poker clean

all: module poker

module:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules

controller.o: controller.c controller.h
	$(CC) $(CFLAGS) -c $<

poker_logic.o: poker_logic.c poker.h controller.h vga_poker.h
	$(CC) $(CFLAGS) -c $<

poker: $(USER_OBJS)
	$(CC) $(CFLAGS) -o $@ $(USER_OBJS) -lusb-1.0 -lrt

clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
	$(RM) *.o poker

endif
