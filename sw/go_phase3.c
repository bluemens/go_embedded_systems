/*
 * go_phase3.c — Phase 3 interactive bring-up
 *
 *   - Opens USB keyboard via libusb (lab2 / FlappyBird pattern).
 *   - mmaps the LW HPS-to-FPGA bridge for go_peripheral access.
 *   - Arrow keys move a green cursor ring on the board.
 *   - Enter places a stone (alternating black/white). NO game logic yet —
 *     no captures, no ko check, no suicide check. Phase 4 will add board.c.
 *   - Esc quits.
 *
 * Build: make go_phase3
 *   (links against libusb-1.0; ensure the package is on the SD card image.)
 *
 * Run as root: sudo ./go_phase3
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "usbkeyboard.h"

/* ─── HW interface ────────────────────────────────────────────────────────── */

#define LW_BRIDGE_BASE   0xFF200000UL
#define LW_BRIDGE_SPAN   0x00200000UL
#define GO_PERIPHERAL_OFFSET  0x0000

#define REG_SET_BLACK    0x00
#define REG_SET_WHITE    0x01
#define REG_CLEAR_CELL   0x02
#define REG_RESET_BOARD  0x03
#define REG_CURSOR       0x04

static volatile uint8_t *go_regs;

static int hw_init(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem (need root?)"); return -1; }
    void *base = mmap(NULL, LW_BRIDGE_SPAN, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (base == MAP_FAILED) { perror("mmap"); close(fd); return -1; }
    go_regs = (volatile uint8_t *)base + GO_PERIPHERAL_OFFSET;
    close(fd);
    return 0;
}

static inline void wr8(uint32_t off, uint8_t val) { go_regs[off] = val; }

static inline int cell_idx(int row, int col) { return row * 9 + col; }

static void cursor_set(int row, int col, int visible)
{
    /* Encoding: bit 7 = visible, bits 6..0 = cell_idx */
    uint8_t v = (visible ? 0x80 : 0x00) | (cell_idx(row, col) & 0x7F);
    wr8(REG_CURSOR, v);
}

static void place_stone(int row, int col, int color /* 1=black, 2=white */)
{
    if (color == 1)      wr8(REG_SET_BLACK, cell_idx(row, col));
    else if (color == 2) wr8(REG_SET_WHITE, cell_idx(row, col));
}

/* ─── HID keycodes [USB HID Usage Tables 1.5, Keyboard/Keypad page 0x07] ─── */
#define KEY_RIGHT   0x4F
#define KEY_LEFT    0x50
#define KEY_DOWN    0x51
#define KEY_UP      0x52
#define KEY_ENTER   0x28
#define KEY_SPACE   0x2C
#define KEY_ESC     0x29
#define KEY_R       0x15

/* ─── Main loop ───────────────────────────────────────────────────────────── */

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

int main(void)
{
    /* Find keyboard */
    uint8_t endpoint;
    struct libusb_device_handle *kbd = openkeyboard(&endpoint);
    if (!kbd) {
        fprintf(stderr, "No USB keyboard found.\n");
        return 1;
    }

    /* Map FPGA registers */
    if (hw_init() != 0) {
        libusb_close(kbd);
        return 1;
    }

    /* Initial state: empty board, cursor at tengen (4,4), Black to play */
    wr8(REG_RESET_BOARD, 1);
    int cursor_row = 4, cursor_col = 4;
    int next_color = 1;     /* 1 = black, 2 = white */
    cursor_set(cursor_row, cursor_col, 1);

    printf("Phase 3 interactive: arrows = move cursor, Enter = place,\n"
           "                     R = reset, Esc = quit.\n");

    struct usb_keyboard_packet pkt;
    int xferred;
    uint8_t prev_key = 0;

    for (;;) {
        int r = libusb_interrupt_transfer(kbd, endpoint,
                                          (unsigned char *)&pkt, sizeof(pkt),
                                          &xferred, 10);
        if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
            fprintf(stderr, "libusb_interrupt_transfer error: %d\n", r);
            break;
        }
        if (xferred != sizeof(pkt)) continue;

        uint8_t key = pkt.keycode[0];
        if (key == prev_key) continue;   /* only react on transitions */
        prev_key = key;
        if (key == 0) continue;          /* key release */

        switch (key) {
        case KEY_UP:    cursor_row = clamp(cursor_row - 1, 0, 8);
                        cursor_set(cursor_row, cursor_col, 1); break;
        case KEY_DOWN:  cursor_row = clamp(cursor_row + 1, 0, 8);
                        cursor_set(cursor_row, cursor_col, 1); break;
        case KEY_LEFT:  cursor_col = clamp(cursor_col - 1, 0, 8);
                        cursor_set(cursor_row, cursor_col, 1); break;
        case KEY_RIGHT: cursor_col = clamp(cursor_col + 1, 0, 8);
                        cursor_set(cursor_row, cursor_col, 1); break;
        case KEY_ENTER:
            place_stone(cursor_row, cursor_col, next_color);
            next_color = (next_color == 1) ? 2 : 1;
            break;
        case KEY_R:
            wr8(REG_RESET_BOARD, 1);
            next_color = 1;
            break;
        case KEY_ESC:
            goto done;
        default:
            break;
        }
    }

done:
    /* Hide cursor on exit so the screen looks tidy */
    cursor_set(cursor_row, cursor_col, 0);
    libusb_close(kbd);
    printf("Bye.\n");
    return 0;
}
