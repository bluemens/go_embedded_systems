/*
 * ai.h — Computer-player API for the Go engine
 *
 * Three levels:
 *   AI_RANDOM — uniform-random over legal moves; passes if none.
 *   AI_GREEDY — heuristic (capture / defend atari / reduce libs / center bias).
 *   AI_MCTS   — UCT-MCTS, 200 simulations, max depth 4×N² = 324 moves per
 *               rollout. Uses simplified rules during rollouts (no ko check,
 *               suicide rejected) for speed.
 *
 * AI_MCTS internally dispatches to one of two implementations selected at
 * startup by ai_init() (see ai_hw.h):
 *   • Tree-MCTS in SW (move_mcts) when g_use_hw_mcts == 0.
 *   • Flat-MCTS via the dispatcher (move_mcts_flat_hw) when g_use_hw_mcts
 *     == 1; that dispatcher then routes leaf evals to ai_mcts_sw_eval or
 *     ai_mcts_hw_eval depending on whether the FPGA accelerator probed OK.
 *
 * Phase 9 adds two demo-facing extensions:
 *   1. ai_toggle_backend / ai_backend_label — flip g_use_hw_mcts from a
 *      keypress (Tab) so users can A/B SW tree MCTS vs HW flat MCTS live.
 *   2. ai_set_mcts_heat_callback — a hook the SW tree MCTS invokes every
 *      8 sims with a snapshot of root-child win-rates, so go_main.c can
 *      stream live heat values to the FPGA overlay tilemap.
 */

#ifndef _AI_H
#define _AI_H

#include "board.h"

typedef enum {
    AI_RANDOM = 1,
    AI_GREEDY = 2,
    AI_MCTS   = 3,
} AiLevel;

typedef struct {
    int row;        /* 0..8 */
    int col;        /* 0..8 */
    int pass;       /* 1 = pass, 0 = real move */
} Move;

/* Returns the AI's move for b->turn at the requested level. */
Move ai_get_move(const BoardState *b, AiLevel level);

/* Seed the AI's PRNG. Call once at startup if you want non-deterministic
 * games; otherwise the AI plays the same way every run (useful for tests). */
void ai_seed(uint32_t seed);

/* ─── Phase 9: demo-facing accessors over the existing g_use_hw_mcts flag */

/* Swap g_use_hw_mcts. Wired to Tab in go_main.c.
 *
 * Note: ai_init() (declared in ai_hw.h) decides the initial value. If the
 * FPGA accelerator probe succeeded, the HW (flat) path is active by default
 * and Tab flips to SW (tree). If the probe failed, HW = SW eval under the
 * hood and the Tab toggle still works mechanically, but both paths run on
 * the CPU; the timer just shows tree-MCTS vs flat-MCTS-on-CPU difference. */
void ai_toggle_backend(void);

/* 1 if g_use_hw_mcts is set, else 0. */
int  ai_backend_is_hw(void);

/* "SW" (tree-MCTS) or "HW" (flat-MCTS via dispatcher). */
const char *ai_backend_label(void);

/* ─── Phase 9: heat-map publish callback (SW tree MCTS only) ─────────────── */

/* Snapshot of root-level statistics, taken every 8 sims during tree MCTS.
 * Indices 0..80 = board cells (row*9+col), 81 = pass.
 *
 *   win_rates[i] = wins[i] / max(visits[i], 1), or 0.0 if never visited.
 *   visits[i]    = total visits to root-child i so far.
 *
 * The callback runs synchronously on the MCTS thread; keep it fast (low µs).
 * The HW flat path doesn't invoke this — it returns in ~1 ms, so live
 * progress wouldn't be visible anyway. */
typedef void (*MctsHeatCallback)(const float win_rates[82],
                                 const int   visits[82]);

void ai_set_mcts_heat_callback(MctsHeatCallback cb);

#endif
