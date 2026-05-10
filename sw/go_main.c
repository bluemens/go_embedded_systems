/*
 * go_main.c — Phase 4: rule-enforced PvP game on the DE1-SoC
 *
 * Same loop shape as go_phase3.c but with board_place() in the path:
 *   keyboard ── arrow ──▶ cursor move
 *           ── Enter ──▶ board_place() → on MOVE_OK, push entire board to HW
 *           ── Space ──▶ board_pass()
 *           ── R     ──▶ board_init() + reset HW + redraw cursor
 *           ── Esc   ──▶ quit
 *
 * Captures, ko, and suicide are all enforced by board.c. After a successful
 * move, all 81 cells are written to the FPGA tilemap (~6 µs total). This is
 * pessimistic — only changed cells need writing — but it's simple and the
 * timing budget is fine.
 *
 * No game-over screen yet (Phase 5 strip framebuffer); on two passes we just
 * compute and print the score, then keep the game in over state until R.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "board.h"
#include "usbkeyboard.h"

/* ─── HW interface (mirrors go_phase3.c) ─────────────────────────────────── */

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
    if (fd < 0) { perror("open /dev/mem"); return -1; }
    void *base = mmap(NULL, LW_BRIDGE_SPAN, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (base == MAP_FAILED) { perror("mmap"); close(fd); return -1; }
    go_regs = (volatile uint8_t *)base + GO_PERIPHERAL_OFFSET;
    close(fd);
    return 0;
}

static inline void wr8(uint32_t off, uint8_t v) { go_regs[off] = v; }
static inline int  cell_idx(int r, int c)       { return r * 9 + c; }

static void hw_reset_board(void) { wr8(REG_RESET_BOARD, 1); }

static void hw_cursor(int row, int col, int visible)
{
    wr8(REG_CURSOR, (visible ? 0x80 : 0) | (cell_idx(row, col) & 0x7F));
}

/* Push the whole board state to FPGA tilemap. ~81 byte writes. */
static void hw_push_board(const BoardState *b)
{
    for (int r = 0; r < BOARD_N; r++)
        for (int c = 0; c < BOARD_N; c++) {
            int idx = cell_idx(r, c);
            switch (b->cells[r][c]) {
                case BLACK: wr8(REG_SET_BLACK,  idx); break;
                case WHITE: wr8(REG_SET_WHITE,  idx); break;
                case EMPTY: wr8(REG_CLEAR_CELL, idx); break;
            }
        }
}

/* ─── HID keycodes ───────────────────────────────────────────────────────── */
#define KEY_RIGHT 0x4F
#define KEY_LEFT  0x50
#define KEY_DOWN  0x51
#define KEY_UP    0x52
#define KEY_ENTER 0x28
#define KEY_SPACE 0x2C
#define KEY_ESC   0x29
#define KEY_R     0x15

static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static const char *moveresult_str(MoveResult r)
{
    switch (r) {
    case MOVE_OK:               return "OK";
    case MOVE_ILLEGAL_OCCUPIED: return "occupied";
    case MOVE_ILLEGAL_SUICIDE:  return "suicide";
    case MOVE_ILLEGAL_KO:       return "ko";
    }
    return "?";
}

static const char *stone_str(Stone s)
{
    return s == BLACK ? "Black" : s == WHITE ? "White" : "?";
}

int main(void)
{
    /* Open keyboard + map FPGA */
    uint8_t endpoint;
    struct libusb_device_handle *kbd = openkeyboard(&endpoint);
    if (!kbd) { fprintf(stderr, "No USB keyboard.\n"); return 1; }
    if (hw_init() != 0) { libusb_close(kbd); return 1; }

    BoardState b;
    board_init(&b);
    hw_reset_board();

    int cursor_row = 4, cursor_col = 4;
    hw_cursor(cursor_row, cursor_col, 1);

    printf("Phase 4 PvP. Black to play.\n"
           "Controls: arrows = cursor, Enter = place,\n"
           "          Space = pass, R = restart, Esc = quit.\n");

    struct usb_keyboard_packet pkt;
    int xferred;
    uint8_t prev_key = 0;

    for (;;) {
        int r = libusb_interrupt_transfer(kbd, endpoint,
                                          (unsigned char *)&pkt, sizeof(pkt),
                                          &xferred, 10);
        if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
            fprintf(stderr, "libusb error: %d\n", r);
            break;
        }
        if (xferred != sizeof(pkt)) continue;

        uint8_t key = pkt.keycode[0];
        if (key == prev_key) continue;
        prev_key = key;
        if (key == 0) continue;

        switch (key) {
        case KEY_UP:    cursor_row = clamp(cursor_row - 1, 0, 8);
                        hw_cursor(cursor_row, cursor_col, 1); break;
        case KEY_DOWN:  cursor_row = clamp(cursor_row + 1, 0, 8);
                        hw_cursor(cursor_row, cursor_col, 1); break;
        case KEY_LEFT:  cursor_col = clamp(cursor_col - 1, 0, 8);
                        hw_cursor(cursor_row, cursor_col, 1); break;
        case KEY_RIGHT: cursor_col = clamp(cursor_col + 1, 0, 8);
                        hw_cursor(cursor_row, cursor_col, 1); break;
        case KEY_ENTER: {
            if (b.game_over) { printf("Game over — press R to restart.\n"); break; }
            Stone moved = b.turn;
            MoveResult mr = board_place(&b, cursor_row, cursor_col);
            if (mr == MOVE_OK) {
                hw_push_board(&b);
                hw_cursor(cursor_row, cursor_col, 1);
                printf("%s plays (%d,%d). Captured: B=%d W=%d. %s to move.\n",
                       stone_str(moved), cursor_row, cursor_col,
                       b.captured_black, b.captured_white,
                       stone_str(b.turn));
            } else {
                printf("Illegal: %s\n", moveresult_str(mr));
            }
            break;
        }
        case KEY_SPACE: {
            if (b.game_over) { printf("Game over — press R to restart.\n"); break; }
            Stone passer = b.turn;
            board_pass(&b);
            printf("%s passes (%d/2). %s to move.\n",
                   stone_str(passer), b.consecutive_passes,
                   stone_str(b.turn));
            if (b.game_over) {
                int blk, wht;
                board_score(&b, &blk, &wht);
                /* Komi: White +5.5 (Chinese rules) */
                printf("\nGAME OVER\n  Black: %d points\n  White: %d + 5.5 komi = %.1f\n",
                       blk, wht, wht + 5.5);
                printf("  Winner: %s\n",
                       (blk > wht + 5.5) ? "Black" : "White");
            }
            break;
        }
        case KEY_R:
            board_init(&b);
            hw_reset_board();
            cursor_row = 4; cursor_col = 4;
            hw_cursor(cursor_row, cursor_col, 1);
            printf("\n--- new game ---\nBlack to move.\n");
            break;
        case KEY_ESC:
            goto done;
        default:
            break;
        }
    }

done:
    hw_cursor(cursor_row, cursor_col, 0);
    libusb_close(kbd);
    printf("Bye.\n");
    return 0;
}
