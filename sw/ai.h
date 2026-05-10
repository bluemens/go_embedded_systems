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
 * All three are pure C and run on the HPS Cortex-A9. The MCTS rollout phase
 * is the candidate for FPGA offload (design-document.md §7.5, Phase 7b);
 * not implemented yet.
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

/* Returns the AI's move for b->turn at the requested level.
 * If no legal moves exist (shouldn't happen except in extreme positions),
 * returns {.pass = 1}. */
Move ai_get_move(const BoardState *b, AiLevel level);

/* Seed the AI's PRNG. Call once at startup if you want non-deterministic
 * games; otherwise the AI plays the same way every run (useful for tests). */
void ai_seed(uint32_t seed);

#endif
