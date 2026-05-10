ifneq (${KERNELRELEASE},)

# KERNELRELEASE defined: we are being compiled as part of the Kernel
obj-m := vga_ball.o

else

# We are being compiled as a module: use the Kernel build system
KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
PWD := $(shell pwd)

default: module main

module:
	$(MAKE) -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

main: main.c controller.c
	$(CC) -Wall -o main main.c controller.c -lusb-1.0

clean:
	$(MAKE) -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	$(RM) main

TARFILES = Makefile README vga_ball.h vga_ball.c main.c controller.c controller.h
TARFILE = lab3-sw.tar.gz
.PHONY : tar
tar : $(TARFILE)

$(TARFILE) : $(TARFILES)
	tar zcfC $(TARFILE) .. $(TARFILES:%=lab3-sw/%)

endif