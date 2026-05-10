ifneq (${KERNELRELEASE},)

#KERNELRELEASE defined: we are being compiled as part of the Kernel
		obj-m := vga_gpu.o

else

#We are being compiled as a module: use the Kernel build system
	KERNEL_SOURCE := /usr/src/linux-headers-$(shell uname -r)
		PWD := $(shell pwd)

TARGET := main

BUILD_DIR := ./build
SRCS := main.c game_map.c physics.c level_reader.c
#SRCS := main.c
TRACKBALL_SRCS := ../trackball/trackball.c

TRACKBALL_OBJS := $(BUILD_DIR)/trackball.o
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o) $(TRAKBALL_OBJS)

DEPS := $(OBJS:.o=.d)

TEST_OBJS := $(BUILD_DIR)/game_map.c.o \
	$(BUILD_DIR)/physics.c.o \
	$(BUILD_DIR)/trackball.o \
	$(BUILD_DIR)/level_reader.c.o

CPPFLAGS = -Wall -g -MMD -MP
LDFLAGS = -pthread

default: module $(TARGET)

$(TARGET): $(OBJS) $(TRACKBALL_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/trackball.o: $(TRACKBALL_SRCS)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: test

module:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} modules

.PHONY: clean
clean:
	${MAKE} -C ${KERNEL_SOURCE} SUBDIRS=${PWD} clean
	${RM} main
	${RM} -r $(BUILD_DIR)


$(TARFILE) : $(TARFILES)
	tar zcfC $(TARFILE) .. $(TARFILES:%=lab3-sw/%)

-include $(DEPS)

endif
