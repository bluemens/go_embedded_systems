ifneq (${KERNELRELEASE},)

# KERNELRELEASE defined: we are being compiled as part of the Kernel
        obj-m := vga_ball.o

else

# We are being compiled as a module: use the Kernel build system

	KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
        PWD := $(shell pwd)

default: module hello go_test go_phase3 go_main go_self_test

module:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

# Phase 2 userspace bring-up tester (no input, just writes a stone pattern).
go_test: go_test.c
	$(CC) -O2 -Wall -o $@ $<

# Phase 3 interactive bring-up: cursor + USB keyboard. Links libusb-1.0.
go_phase3: go_phase3.c usbkeyboard.c usbkeyboard.h
	$(CC) -O2 -Wall -o $@ go_phase3.c usbkeyboard.c -lusb-1.0

# AI + MCTS HW dispatch library — shared between go_main and go_self_test.
AI_SRC = ai.c ai_mcts_hw.c hw_iface.c board.c

# Phase 7b: PvP + PvC with MCTS HW dispatch.
go_main: go_main.c usbkeyboard.c usbkeyboard.h board.c board.h \
         strip_render.c strip_render.h font5x7.c font5x7.h \
         ai.c ai.h ai_hw.h ai_mcts_hw.c hw_iface.c hw_iface.h \
         hw_timer.c hw_timer.h
	$(CC) -O2 -Wall -o $@ go_main.c usbkeyboard.c $(AI_SRC) \
	      strip_render.c font5x7.c hw_timer.c -lusb-1.0 -lm

# MCTS HW smoke test: runs hw_mcts_self_test() and reports pass/fail.
# Useful at first-boot bring-up; no USB / display required.
go_self_test: go_self_test.c $(AI_SRC) ai.h ai_hw.h hw_iface.h board.h
	$(CC) -O2 -Wall -o $@ go_self_test.c $(AI_SRC) -lm

# Host-runnable unit tests for the rule engine. No HW deps; build on Mac/Linux.
board_test: board_test.c board.c board.h
	$(CC) -O2 -Wall -o $@ board_test.c board.c

clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	${RM} hello go_test go_phase3 go_main go_self_test board_test

TARFILES = Makefile README vga_ball.h vga_ball.c hello.c
TARFILE = lab3-sw.tar.gz
.PHONY : tar
tar : $(TARFILE)

$(TARFILE) : $(TARFILES)
	tar zcfC $(TARFILE) .. $(TARFILES:%=lab3-sw/%)

endif 
