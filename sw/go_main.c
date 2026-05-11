/*
 * go_main.c — Phase 8: full game flow
 *
 * UI state machine:
 *   TITLE          → "GO 9x9 — Press ENTER to start"
 *   MODE_SELECT    → PvP / PvC (left/right + Enter)
 *   DIFFICULTY_SEL → Level 1 / 2 / 3 (PvC only)
 *   GAME           → live game, Phase 4–7 logic, score panel updates
 *
 * Esc quits from any state. R restarts back to TITLE from GAME / GAME_OVER.
 *
 * argv shortcut: ./go_main [N]
 *   N absent     → start at TITLE
 *   N=1/2/3      → skip menus; PvC at that level
 *   N=0          → skip menus; PvP
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "board.h"
#include "ai.h"
#include "usbkeyboard.h"
#include "strip_render.h"
#include <time.h>

/* ─── HW interface ───────────────────────────────────────────────────────── */

#define LW_BRIDGE_BASE   0xFF200000UL
#define LW_BRIDGE_SPAN   0x00200000UL
#define GO_PERIPHERAL_OFFSET  0x0000        /* avalon_slave_0 */
#define STRIP_FB_OFFSET       0x10000       /* avalon_slave_1 (set in Qsys) */

#define REG_SET_BLACK     0x00
#define REG_SET_WHITE     0x01
#define REG_CLEAR_CELL    0x02
#define REG_RESET_BOARD   0x03
#define REG_CURSOR        0x04
#define REG_STRIP_SWAP    0x05
#define REG_AUDIO_CMD     0x06
#define REG_AUDIO_STATUS  0x07

#define AUDIO_NONE      0
#define AUDIO_PLACE     1
#define AUDIO_CAPTURE   2
#define AUDIO_ILLEGAL   3
#define AUDIO_GAME_OVER 4

static volatile uint8_t *go_regs;
static volatile uint8_t *lw_base;     /* shared with strip_render */

static int hw_init(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return -1; }
    void *base = mmap(NULL, LW_BRIDGE_SPAN, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, LW_BRIDGE_BASE);
    if (base == MAP_FAILED) { perror("mmap"); close(fd); return -1; }
    lw_base  = (volatile uint8_t *)base;
    go_regs  = lw_base + GO_PERIPHERAL_OFFSET;
    close(fd);
    return 0;
}

static inline void wr8(uint32_t off, uint8_t v) { go_regs[off] = v; }
static inline int  cell_idx(int r, int c)       { return r * 9 + c; }

static void hw_reset_board(void) { wr8(REG_RESET_BOARD, 1); }

static inline void hw_play_audio(uint8_t cmd) { wr8(REG_AUDIO_CMD, cmd); }

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

/* ─── Menu screens ──────────────────────────────────────────────────────── */

/* Center one option of an N-option row at fixed slot `i` of `n` slots,
 * spaced evenly across the strip. Returns the x coordinate at which to
 * begin painting a glyph or box of width `item_w`. */
static int slot_x(int i, int n, int item_w)
{
    /* Slot center: STRIP_W * (i + 0.5) / n. */
    int center = (STRIP_W * (2 * i + 1)) / (2 * n);
    return center - item_w / 2;
}

static void render_title(void)
{
    strip_clear(COLOR_STRIP_BG);
    strip_text_centered(2,  "GO 9X9",      5, COLOR_STRIP_GOLD,  COLOR_STRIP_BG);
    strip_text_centered(46, "PRESS ENTER", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    strip_present();
}

/* Paint one menu option at slot (i,n). When `selected`, draws a filled gold
 * pill (dark text on gold bg); otherwise plain white-on-bg. */
static void draw_menu_option(int i, int n, int y, const char *label,
                             int scale, int selected)
{
    int pad = 4;
    int w   = strip_text_width(label, scale) + 2 * pad;
    int x   = slot_x(i, n, w);
    if (selected) {
        strip_text_box(x, y, label, scale,
                       COLOR_STRIP_BG, COLOR_STRIP_GOLD, pad);
    } else {
        strip_text(x + pad, y + pad, label, scale,
                   COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    }
}

static void render_mode_select(int sel /* 0=PvP, 1=PvC */)
{
    strip_clear(COLOR_STRIP_BG);
    strip_text_centered(2, "SELECT MODE", 2,
                        COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    draw_menu_option(0, 2, 18, "PVP", 3, sel == 0);
    draw_menu_option(1, 2, 18, "PVC", 3, sel == 1);
    strip_text_centered(52, "<- -> SELECT   ENTER OK   ESC BACK", 1,
                        COLOR_STRIP_GRAY, COLOR_STRIP_BG);
    strip_present();
}

static void render_difficulty(int sel /* 0..2 */)
{
    strip_clear(COLOR_STRIP_BG);
    strip_text_centered(2, "AI DIFFICULTY", 2,
                        COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    const char *labels[3] = { "L1", "L2", "L3" };
    for (int i = 0; i < 3; i++)
        draw_menu_option(i, 3, 18, labels[i], 3, sel == i);
    strip_text_centered(52, "<- -> SELECT   ENTER OK   ESC BACK", 1,
                        COLOR_STRIP_GRAY, COLOR_STRIP_BG);
    strip_present();
}

static void render_panel(const BoardState *b)
{
    char buf[64];
    strip_clear(COLOR_STRIP_BG);

    if (b->game_over) {
        int blk, wht;
        board_score(b, &blk, &wht);
        double w_total = wht + 5.5;
        const char *winner = (blk > w_total) ? "BLACK" : "WHITE";

        strip_text_centered(2, "GAME OVER", 3,
                            COLOR_STRIP_GOLD, COLOR_STRIP_BG);
        snprintf(buf, sizeof(buf),
                 "BLACK %d   WHITE %d+5.5   WINNER %s",
                 blk, wht, winner);
        strip_text_centered(30, buf, 2,
                            COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        strip_text_centered(52, "PRESS R NEW GAME   ESC QUIT", 1,
                            COLOR_STRIP_GRAY, COLOR_STRIP_BG);
    } else {
        /* Row 1 (y=4): turn indicator, prominent. */
        snprintf(buf, sizeof(buf), "TURN  %s",
                 b->turn == BLACK ? "BLACK" : "WHITE");
        strip_text_centered(4, buf, 2,
                            b->turn == BLACK ? COLOR_STRIP_WHITE
                                             : COLOR_STRIP_BURLY,
                            COLOR_STRIP_BG);

        /* Row 2 (y=26): captures + pass indicator. */
        if (b->consecutive_passes == 1) {
            snprintf(buf, sizeof(buf),
                     "CAPTURED  B %d   W %d     1 PASS",
                     b->captured_white, b->captured_black);
        } else {
            snprintf(buf, sizeof(buf),
                     "CAPTURED  B %d   W %d",
                     b->captured_white, b->captured_black);
        }
        strip_text_centered(26, buf, 2,
                            COLOR_STRIP_GRAY, COLOR_STRIP_BG);

        /* Row 3 (y=50): controls hint. */
        strip_text_centered(50,
            "ENTER PLACE   SPACE PASS   R MENU   ESC QUIT", 1,
            COLOR_STRIP_GRAY, COLOR_STRIP_BG);
    }

    strip_present();
}

/* Apply an AI move to the BoardState; sync HW; print log. */
static void apply_ai_move(BoardState *b, AiLevel level,
                          int cursor_row, int cursor_col)
{
    Stone moved = b->turn;
    int prev_caps = b->captured_black + b->captured_white;
    Move m = ai_get_move(b, level);
    if (m.pass) {
        board_pass(b);
        printf("AI (%s) passes.\n", stone_str(moved));
    } else {
        MoveResult r = board_place(b, m.row, m.col);
        if (r != MOVE_OK) {
            /* AI returned an illegal move — shouldn't happen with our
             * legal_moves() generator, but be defensive: pass instead. */
            board_pass(b);
            printf("AI tried illegal (%d,%d), passes instead.\n", m.row, m.col);
        } else {
            int new_caps = b->captured_black + b->captured_white;
            hw_play_audio(new_caps > prev_caps ? AUDIO_CAPTURE : AUDIO_PLACE);
            printf("AI (%s) plays (%d,%d).\n", stone_str(moved), m.row, m.col);
        }
    }
    hw_push_board(b);
    hw_cursor(cursor_row, cursor_col, 1);
    render_panel(b);
}

typedef enum {
    UI_TITLE,
    UI_MODE_SELECT,
    UI_DIFFICULTY,
    UI_GAME,
} UiState;

/* Helpers to enter each state cleanly (resets HW + redraws strip). */
static void enter_title(void)
{
    hw_reset_board();
    hw_cursor(0, 0, 0);
    render_title();
}
static void enter_mode_select(int sel)
{
    hw_reset_board();
    hw_cursor(0, 0, 0);
    render_mode_select(sel);
}
static void enter_difficulty(int sel)
{
    render_difficulty(sel);
}
static void enter_game(BoardState *b, int *crow, int *ccol)
{
    board_init(b);
    hw_reset_board();
    *crow = 4; *ccol = 4;
    hw_cursor(*crow, *ccol, 1);
    render_panel(b);
}

int main(int argc, char **argv)
{
    /* CLI shortcuts:
     *   no arg     → start at TITLE menu
     *   0          → skip menus, PvP
     *   1 / 2 / 3  → skip menus, PvC at level N */
    AiLevel ai_level    = 0;
    int     pvc         = 0;
    int     skip_menu   = 0;
    if (argc >= 2) {
        int n = atoi(argv[1]);
        if (n == 0)                  { skip_menu = 1; pvc = 0; }
        else if (n >= 1 && n <= 3)   { skip_menu = 1; pvc = 1; ai_level = (AiLevel)n; }
    }
    ai_seed((uint32_t)time(NULL));

    /* Open keyboard + map FPGA */
    uint8_t endpoint;
    struct libusb_device_handle *kbd = openkeyboard(&endpoint);
    if (!kbd) { fprintf(stderr, "No USB keyboard.\n"); return 1; }
    if (hw_init() != 0) { libusb_close(kbd); return 1; }
    strip_init(lw_base, STRIP_FB_OFFSET, GO_PERIPHERAL_OFFSET + REG_STRIP_SWAP);

    BoardState b;
    int cursor_row = 4, cursor_col = 4;
    int mode_sel = 0;          /* 0 = PvP, 1 = PvC */
    int diff_sel = 1;          /* 0..2 → level 1..3 */
    UiState ui;

    if (skip_menu) {
        ui = UI_GAME;
        enter_game(&b, &cursor_row, &cursor_col);
        printf(pvc
               ? "PvC (AI=White, level %d). Black (you) to play.\n"
               : "PvP. Black to play.\n",
               ai_level);
    } else {
        ui = UI_TITLE;
        enter_title();
        printf("Phase 8: title menu. Enter to start, Esc to quit.\n");
    }

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

        if (key == KEY_ESC) {
            if (ui == UI_TITLE || ui == UI_GAME) goto done;
            if (ui == UI_MODE_SELECT) { ui = UI_TITLE; enter_title(); continue; }
            if (ui == UI_DIFFICULTY)  { ui = UI_MODE_SELECT;
                                        enter_mode_select(mode_sel); continue; }
        }

        switch (ui) {
        /* ─────────────── TITLE ─────────────── */
        case UI_TITLE:
            if (key == KEY_ENTER) {
                ui = UI_MODE_SELECT;
                enter_mode_select(mode_sel);
            }
            break;

        /* ─────────────── MODE_SELECT ─────────────── */
        case UI_MODE_SELECT:
            if (key == KEY_LEFT)  { mode_sel = 0; render_mode_select(mode_sel); }
            if (key == KEY_RIGHT) { mode_sel = 1; render_mode_select(mode_sel); }
            if (key == KEY_ENTER) {
                if (mode_sel == 0) {
                    pvc = 0; ai_level = 0;
                    ui = UI_GAME;
                    enter_game(&b, &cursor_row, &cursor_col);
                    printf("\nPvP. Black to play.\n");
                } else {
                    ui = UI_DIFFICULTY;
                    enter_difficulty(diff_sel);
                }
            }
            break;

        /* ─────────────── DIFFICULTY ─────────────── */
        case UI_DIFFICULTY:
            if (key == KEY_LEFT  && diff_sel > 0) { diff_sel--; render_difficulty(diff_sel); }
            if (key == KEY_RIGHT && diff_sel < 2) { diff_sel++; render_difficulty(diff_sel); }
            if (key == KEY_ENTER) {
                pvc = 1; ai_level = (AiLevel)(diff_sel + 1);
                ui = UI_GAME;
                enter_game(&b, &cursor_row, &cursor_col);
                printf("\nPvC (AI=White, level %d). Black to play.\n", ai_level);
            }
            break;

        /* ─────────────── GAME ─────────────── */
        case UI_GAME:
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
                if (b.game_over) { printf("Game over — press R to return to menu.\n"); break; }
                if (pvc && b.turn == WHITE) {
                    printf("AI is thinking; please wait.\n");
                    break;
                }
                Stone moved = b.turn;
                int prev_caps = b.captured_black + b.captured_white;
                MoveResult mr = board_place(&b, cursor_row, cursor_col);
                if (mr == MOVE_OK) {
                    int new_caps = b.captured_black + b.captured_white;
                    hw_push_board(&b);
                    hw_cursor(cursor_row, cursor_col, 1);
                    render_panel(&b);
                    hw_play_audio(new_caps > prev_caps ? AUDIO_CAPTURE : AUDIO_PLACE);
                    printf("%s plays (%d,%d). Captured: B=%d W=%d.\n",
                           stone_str(moved), cursor_row, cursor_col,
                           b.captured_black, b.captured_white);
                    if (pvc && !b.game_over && b.turn == WHITE) {
                        printf("AI (level %d) thinking...\n", ai_level);
                        fflush(stdout);
                        apply_ai_move(&b, ai_level, cursor_row, cursor_col);
                    }
                } else {
                    hw_play_audio(AUDIO_ILLEGAL);
                    printf("Illegal: %s\n", moveresult_str(mr));
                }
                break;
            }
            case KEY_SPACE: {
                if (b.game_over) { printf("Game over — press R to return to menu.\n"); break; }
                Stone passer = b.turn;
                board_pass(&b);
                render_panel(&b);
                printf("%s passes (%d/2). %s to move.\n",
                       stone_str(passer), b.consecutive_passes,
                       stone_str(b.turn));
                if (b.game_over) {
                    int blk, wht;
                    board_score(&b, &blk, &wht);
                    hw_play_audio(AUDIO_GAME_OVER);
                    printf("\nGAME OVER\n  Black: %d points\n  White: %d + 5.5 komi = %.1f\n",
                           blk, wht, wht + 5.5);
                    printf("  Winner: %s\n",
                           (blk > wht + 5.5) ? "Black" : "White");
                }
                break;
            }
            case KEY_R:
                /* From GAME / GAME_OVER: back to TITLE menu (or skip back to
                 * GAME if launched with the argv shortcut). */
                if (skip_menu) {
                    enter_game(&b, &cursor_row, &cursor_col);
                    printf("\n--- new game ---\n");
                } else {
                    ui = UI_TITLE;
                    enter_title();
                    printf("\n--- back to menu ---\n");
                }
                break;
            default: break;
            }
            break;
        }
    }

done:
    hw_cursor(cursor_row, cursor_col, 0);
    libusb_close(kbd);
    printf("Bye.\n");
    return 0;
}
