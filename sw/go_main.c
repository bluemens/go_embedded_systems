/*
 * go_main.c — Phase 8 + Phase 9: full game flow with live demo features
 *
 * UI state machine:
 *   TITLE          → "GO 9x9 — Press ENTER to start"
 *   MODE_SELECT    → PvP / PvC (left/right + Enter)
 *   DIFFICULTY_SEL → Level 1 / 2 / 3 (PvC only)
 *   GAME           → live game, Phase 4–7 logic, score panel updates
 *
 * Esc quits from any state. R restarts back to TITLE from GAME / GAME_OVER.
 *
 * Phase 9 keys (PvC + Level 3 only):
 *   Tab   — toggle AI backend (SW tree-MCTS  ⇄  HW flat-MCTS)
 *   Y     — replay the previous AI move with the *other* backend
 *           on the same board, for a live side-by-side speedup demo.
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
#include "ai_hw.h"
#include "hw_timer.h"
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
/* Phase 9: heat-map overlay registers (timer regs live in hw_timer.c). */
#define REG_HEAT_IDX      0x08
#define REG_HEAT_VAL      0x09
#define REG_OVERLAY_EN    0x0A
#define REG_HEAT_CLEAR    0x0B

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

/* ─── Phase 9: heat-map overlay helpers ──────────────────────────────────── */

static void hw_set_heat(int cell, int level)
{
    /* Two-write protocol: IDX latches address, VAL commits {addr, value}. */
    wr8(REG_HEAT_IDX, (uint8_t)(cell  & 0x7F));
    wr8(REG_HEAT_VAL, (uint8_t)(level & 0x0F));
}
static inline void hw_overlay_enable(int on) { wr8(REG_OVERLAY_EN, on ? 1 : 0); }
static inline void hw_heat_clear(void)       { wr8(REG_HEAT_CLEAR, 1); }

/* MctsHeatCallback. Win-rate ∈ [0,1] maps to heat level ∈ [1,15] for
 * visited cells; level 0 means "never visited" and shows no tint.
 *
 * Index 81 is the pass move — we don't render it on the board, so we
 * just skip it. Calls 81 cells × 2 byte writes ≈ 162 writes × ~4 LW cycles
 * ≈ 13 µs total, comfortably inside the per-snapshot budget. */
static void heat_cb(const float win_rates[82], const int visits[82])
{
    for (int i = 0; i < 81; i++) {
        int level;
        if (visits[i] == 0) {
            level = 0;
        } else {
            int q = (int)(win_rates[i] * 15.0f + 0.5f);
            if (q < 1)  q = 1;
            if (q > 15) q = 15;
            level = q;
        }
        hw_set_heat(i, level);
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
/* Phase 9: live-demo keys. */
#define KEY_TAB   0x2B
#define KEY_Y     0x1C

/* ─── Phase 9: AI timing + replay state ──────────────────────────────────── */

/* Most-recent AI move's timing (in 50 MHz cycles) and which backend ran. */
static uint32_t last_ai_cycles = 0;
static char     last_ai_label[8]  = "";

/* Second-most-recent: shown alongside "LAST" when the two backends differ
 * (e.g. after pressing Y to replay with the opposite backend). */
static uint32_t prev_ai_cycles = 0;
static char     prev_ai_label[8]  = "";

/* Board state captured immediately before the last AI move, so KEY_Y can
 * rewind and re-run the same position on the other backend. */
static BoardState replay_snapshot;
static int        replay_valid = 0;

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

        /* Row 3 (y=41): Phase 9 backend label + last/prev AI timing.
         * Hidden when no AI move has been observed yet (PvP mode, fresh
         * game). When LAST and PREV are from different backends, also
         * shows a "Nx" speedup factor. */
        if (last_ai_cycles > 0) {
            double last_ms = hw_timer_cycles_to_ms(last_ai_cycles);
            if (prev_ai_cycles > 0
                && strcmp(prev_ai_label, last_ai_label) != 0) {
                double prev_ms = hw_timer_cycles_to_ms(prev_ai_cycles);
                double speedup = (last_ms > 0.0) ? prev_ms / last_ms : 0.0;
                snprintf(buf, sizeof(buf),
                         "%s %.2fMS  PREV %s %.2fMS  %.0fX",
                         last_ai_label, last_ms,
                         prev_ai_label, prev_ms, speedup);
            } else {
                snprintf(buf, sizeof(buf), "BACKEND %s  LAST %.2f MS",
                         ai_backend_label(), last_ms);
            }
            strip_text_centered(41, buf, 1,
                                COLOR_STRIP_GREEN, COLOR_STRIP_BG);
        } else {
            snprintf(buf, sizeof(buf), "BACKEND %s", ai_backend_label());
            strip_text_centered(41, buf, 1,
                                COLOR_STRIP_GREEN, COLOR_STRIP_BG);
        }

        /* Row 4 (y=50): controls hint (Phase 9: + TAB / Y). */
        strip_text_centered(50,
            "ENTER PLAY  SPC PASS  TAB SWAP  Y REPLAY  R MENU  ESC QUIT", 1,
            COLOR_STRIP_GRAY, COLOR_STRIP_BG);
    }

    strip_present();
}

/* Apply an AI move to the BoardState; sync HW; print log.
 *
 * Phase 9: wraps the AI call with the HW cycle counter for live timing,
 * enables the heat overlay during the call (the SW tree-MCTS publishes
 * heat via the installed callback; the HW flat path runs too fast for
 * meaningful live updates and just gets a brief flash), and saves the
 * pre-move board into replay_snapshot so KEY_Y can rewind. */
static void apply_ai_move(BoardState *b, AiLevel level,
                          int cursor_row, int cursor_col)
{
    Stone moved = b->turn;
    int prev_caps = b->captured_black + b->captured_white;

    /* Snapshot before mutating, so KEY_Y can re-run on the same board. */
    replay_snapshot = *b;
    replay_valid    = 1;

    /* Roll "LAST" → "PREV" so the strip can show the speedup pair. */
    prev_ai_cycles = last_ai_cycles;
    strncpy(prev_ai_label, last_ai_label, sizeof(prev_ai_label));
    prev_ai_label[sizeof(prev_ai_label) - 1] = '\0';

    /* Heat overlay live during the AI call. Cleared (cells zeroed) first
     * so a previous turn's heat doesn't flash. Enable, run, disable. */
    if (level == AI_MCTS) {
        hw_heat_clear();
        hw_overlay_enable(1);
    }
    hw_timer_start();
    Move m = ai_get_move(b, level);
    last_ai_cycles = hw_timer_stop_cycles();
    hw_overlay_enable(0);
    strncpy(last_ai_label, ai_backend_label(), sizeof(last_ai_label));
    last_ai_label[sizeof(last_ai_label) - 1] = '\0';

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
            printf("AI (%s) plays (%d,%d).  [%s, %.2f ms]\n",
                   stone_str(moved), m.row, m.col,
                   last_ai_label,
                   hw_timer_cycles_to_ms(last_ai_cycles));
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
    hw_heat_clear();
    hw_overlay_enable(0);
    replay_valid       = 0;
    last_ai_cycles     = 0;
    last_ai_label[0]   = '\0';
    prev_ai_cycles     = 0;
    prev_ai_label[0]   = '\0';
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
    ai_init();   /* probes MCTS accelerator; routes Level 3 to HW if present */

    /* Open keyboard + map FPGA */
    uint8_t endpoint;
    struct libusb_device_handle *kbd = openkeyboard(&endpoint);
    if (!kbd) { fprintf(stderr, "No USB keyboard.\n"); return 1; }
    if (hw_init() != 0) { libusb_close(kbd); return 1; }
    strip_init(lw_base, STRIP_FB_OFFSET, GO_PERIPHERAL_OFFSET + REG_STRIP_SWAP);

    /* Phase 9: bind the HW timer to our go_regs base, and install the
     * heat-publish callback so SW tree-MCTS pushes live overlay updates. */
    hw_timer_init((volatile uint8_t *)go_regs);
    hw_overlay_enable(0);
    hw_heat_clear();
    ai_set_mcts_heat_callback(heat_cb);
    printf("Phase 9: AI backend = %s (Tab toggles, Y replays).\n",
           ai_backend_label());

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
            /* ─── Phase 9: backend toggle ─────────────────────────────── */
            case KEY_TAB:
                if (pvc && ai_level == AI_MCTS) {
                    ai_toggle_backend();
                    printf("AI backend → %s\n", ai_backend_label());
                    render_panel(&b);
                }
                break;
            /* ─── Phase 9: replay last AI move with the other backend ── */
            case KEY_Y: {
                if (!(pvc && ai_level == AI_MCTS)) break;
                if (!replay_valid) {
                    printf("Replay: nothing to replay yet.\n");
                    break;
                }
                if (b.game_over) {
                    printf("Replay: game is over.\n");
                    break;
                }
                /* Rewind the board to the moment before the previous AI
                 * call, toggle the backend, then re-issue the move. The
                 * speedup row in render_panel picks this up because LAST
                 * and PREV will now carry different labels. */
                b = replay_snapshot;
                replay_valid = 0;            /* prevent infinite Y-loops */
                ai_toggle_backend();
                printf("Replay (%s) on same board...\n", ai_backend_label());
                fflush(stdout);
                hw_push_board(&b);
                hw_cursor(cursor_row, cursor_col, 1);
                apply_ai_move(&b, ai_level, cursor_row, cursor_col);
                break;
            }
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
