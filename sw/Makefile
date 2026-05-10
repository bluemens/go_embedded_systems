ifneq (${KERNELRELEASE},)

# KERNELRELEASE defined: we are being compiled as part of the Kernel
        obj-m := vga_ball.o

else

# We are being compiled as a module: use the Kernel build system

	KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
        PWD := $(shell pwd)

default: module hello go_test go_phase3 go_main

module:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

# Phase 2 userspace bring-up tester (no input, just writes a stone pattern).
go_test: go_test.c
	$(CC) -O2 -Wall -o $@ $<

# Phase 3 interactive bring-up: cursor + USB keyboard. Links libusb-1.0.
go_phase3: go_phase3.c usbkeyboard.c usbkeyboard.h
	$(CC) -O2 -Wall -o $@ go_phase3.c usbkeyboard.c -lusb-1.0

# Phase 4 rule-enforced PvP. Adds board.c (game logic).
go_main: go_main.c usbkeyboard.c usbkeyboard.h board.c board.h
	$(CC) -O2 -Wall -o $@ go_main.c usbkeyboard.c board.c -lusb-1.0

# Host-runnable unit tests for the rule engine. No HW deps; build on Mac/Linux.
# Run `make board_test && ./board_test` from any machine, not just the DE1-SoC.
board_test: board_test.c board.c board.h
	$(CC) -O2 -Wall -o $@ board_test.c board.c

clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	${RM} hello go_test go_phase3 go_main board_test

TARFILES = Makefile README vga_ball.h vga_ball.c hello.c
TARFILE = lab3-sw.tar.gz
.PHONY : tar
tar : $(TARFILE)

$(TARFILE) : $(TARFILES)
	tar zcfC $(TARFILE) .. $(TARFILES:%=lab3-sw/%)

endif 
