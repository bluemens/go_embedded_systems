#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libusb-1.0/libusb.h>
#include "usbkeyboard.h"

#define REPORT_LEN         8
#define LW_BRIDGE_BASE     0xFF200000
#define MAP_SIZE           0x1000

#define DINO_Y_OFFSET      (1 * 4)
#define DUCKING_OFFSET     (13 * 4)
#define JUMPING_OFFSET     (14 * 4)
#define REPLAY_OFFSET      (19 * 4)

#define GROUND_Y           248
#define FIXED_SHIFT        4                   
#define GROUND_Y_FIXED     (GROUND_Y << FIXED_SHIFT)

#define INITIAL_VELOCITY   (-84)               
#define GRAVITY            1                   
#define DELAY_US           5000

int main(void) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open(/dev/mem)"); return 1; }
    void *lw_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (lw_base == MAP_FAILED) { perror("mmap"); return 1; }

    volatile uint32_t *dino_y_reg = (uint32_t *)(lw_base + DINO_Y_OFFSET);
    volatile uint32_t *duck_reg   = (uint32_t *)(lw_base + DUCKING_OFFSET);
    volatile uint32_t *jump_reg   = (uint32_t *)(lw_base + JUMPING_OFFSET);
    volatile uint32_t *replay_reg = (uint32_t *)(lw_base + REPLAY_OFFSET);

    struct libusb_device_handle *pad;
    uint8_t ep;
    pad = openkeyboard(&ep);
    if (!pad) {
        fprintf(stderr, "Controller not found\n");
        munmap(lw_base, MAP_SIZE);
        close(fd);
        return 1;
    }

    int y_fixed = GROUND_Y_FIXED;
    int v_fixed = 0;

    unsigned char report[REPORT_LEN];
    int transferred, r;

    while (1) {
        r = libusb_interrupt_transfer(pad, ep, report, REPORT_LEN, &transferred, 0);
        if (r < 0) {
            fprintf(stderr, "USB read error: %d\n", r);
            break;
        }

        uint8_t y_axis = report[4];
        bool want_jump = (y_axis == 0x00 && y_fixed == GROUND_Y_FIXED);
        bool want_duck = (y_axis == 0xFF && y_fixed == GROUND_Y_FIXED);
        bool want_replay = (report[6] & 0x20);

        if (want_jump) v_fixed = INITIAL_VELOCITY;

        v_fixed += GRAVITY;
        y_fixed += v_fixed;

        if (y_fixed > GROUND_Y_FIXED) {
            y_fixed = GROUND_Y_FIXED;
            v_fixed = 0;
        }

        *dino_y_reg = y_fixed >> FIXED_SHIFT;
        *jump_reg = want_jump;
        *duck_reg = want_duck;
        *replay_reg = want_replay;

        usleep(DELAY_US);
    }

    libusb_close(pad);
    libusb_exit(NULL);
    munmap(lw_base, MAP_SIZE);
    close(fd);
    return 0;
}
