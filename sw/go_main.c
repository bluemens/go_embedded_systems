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

/* ─── Strip score panel rendering ────────────────────────────────────────── */

static void itoa2(int v, char *buf)   /* up to 3 digits, no clipping */
{
    if (v < 10)       { buf[0] = '0' + v;             buf[1] = 0; }
    else if (v < 100) { buf[0] = '0' + v/10;
                        buf[1] = '0' + v%10;          buf[2] = 0; }
    else              { buf[0] = '0' + v/100;
                        buf[1] = '0' + (v/10) % 10;
                        buf[2] = '0' + v%10;          buf[3] = 0; }
}

/* ─── Menu screens ──────────────────────────────────────────────────────── */

static void render_title(void)
{
    strip_clear(COLOR_STRIP_BG);
    /* Banner: "GO 9X9" centered, scale 4 → 5*4=20 wide × 7*4=28 tall.
     * 6 chars × (5+1)*4 = 144 px wide; centered at x = (640-144)/2 = 248. */
    strip_text(248, 4, "GO 9X9", 4, COLOR_STRIP_GOLD, COLOR_STRIP_BG);
    strip_text(180, 42, "PRESS ENTER", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    strip_present();
}

static void render_mode_select(int sel /* 0=PvP, 1=PvC */)
{
    strip_clear(COLOR_STRIP_BG);
    strip_text(140, 4, "SELECT MODE", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    /* Two options at scale 3, ~30 px advance each, ~30 px tall. */
    int x_pvp = 140, x_pvc = 360, y = 28;
    uint8_t cpvp = sel == 0 ? COLOR_STRIP_GREEN : COLOR_STRIP_GRAY;
    uint8_t cpvc = sel == 1 ? COLOR_STRIP_GREEN : COLOR_STRIP_GRAY;
    strip_text(x_pvp, y, "PVP", 3, cpvp, COLOR_STRIP_BG);
    strip_text(x_pvc, y, "PVC", 3, cpvc, COLOR_STRIP_BG);
    strip_present();
}

static void render_difficulty(int sel /* 0..2 */)
{
    strip_clear(COLOR_STRIP_BG);
    strip_text(180, 4, "AI DIFFICULTY", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
    int x[3] = { 100, 290, 480 };
    const char *labels[3] = { "L1", "L2", "L3" };
    for (int i = 0; i < 3; i++) {
        uint8_t col = (sel == i) ? COLOR_STRIP_GREEN : COLOR_STRIP_GRAY;
        strip_text(x[i], 28, labels[i], 3, col, COLOR_STRIP_BG);
    }
    strip_present();
}

static void render_panel(const BoardState *b)
{
    char num[8];
    strip_clear(COLOR_STRIP_BG);

    if (b->game_over) {
        int blk, wht;
        board_score(b, &blk, &wht);
        double w_total = wht + 5.5;
        const char *winner = (blk > w_total) ? "BLACK WINS" : "WHITE WINS";

        /* "GAME OVER" big and centered (scale 3 → ~13px wide × 21 tall) */
        strip_text(140, 4, "GAME OVER", 3,
                   COLOR_STRIP_GOLD, COLOR_STRIP_BG);
        /* score line below */
        strip_text(80, 38, "B ", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        itoa2(blk, num);
        strip_text(108, 38, num, 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        strip_text(160, 38, "W ", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        itoa2(wht, num);
        strip_text(188, 38, num, 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        strip_text(240, 38, " 5 KOMI ", 2, COLOR_STRIP_GRAY, COLOR_STRIP_BG);
        strip_text(380, 38, winner, 2, COLOR_STRIP_GOLD, COLOR_STRIP_BG);
    } else {
        /* Live score line, scale 2 (12px advance). */
        strip_text(8,   8, "BLACK:", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        itoa2(b->captured_black, num);    /* white captured BY black? — no: */
        /* captured_black = #black stones captured by W. Show in W's column. */
        strip_text(180, 8, "WHITE:", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);
        strip_text(360, 8, "TURN:", 2, COLOR_STRIP_WHITE, COLOR_STRIP_BG);

        strip_text(8,  32, "CAP B=", 2, COLOR_STRIP_GRAY, COLOR_STRIP_BG);
        itoa2(b->captured_black, num);
        strip_text(92, 32, num, 2, COLOR_STRIP_GRAY, COLOR_STRIP_BG);
        strip_text(140, 32, "W=", 2, COLOR_STRIP_GRAY, COLOR_STRIP_BG);
        itoa2(b->captured_white, num);
        strip_text(168, 32, num, 2, COLOR_STRIP_GRAY, COLOR_STRIP_BG);

        strip_text(440, 8,
                   b->turn == BLACK ? "BLACK" : "WHITE",
                   2,
                   b->turn == BLACK ? COLOR_STRIP_WHITE : COLOR_STRIP_BURLY,
                   COLOR_STRIP_BG);

        if (b->consecutive_passes == 1) {
            strip_text(380, 32, "1 PASS", 2,
                       COLOR_STRIP_GREEN, COLOR_STRIP_BG);
        }
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
