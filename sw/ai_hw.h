/*
 * ai_hw.h — MCTS hardware acceleration interface.
 *
 * The HW path streams a leaf board to the FPGA accelerator, kicks N parallel
 * rollouts, and reads back per-cell (wins, visits) statistics. Falls back to
 * the SW eval via a function pointer in ai.c if hw_init_mcts() fails.
 *
 * Register map matches hw/ip/mcts_accel/mcts_accel.sv and design-doc §7.5.8.
 */
#ifndef _AI_HW_H
#define _AI_HW_H

#include <stdint.h>

#include "board.h"

#define MCTS_RESULT_N 82   /* 81 cells + pass */

typedef struct {
    uint16_t wins;
    uint16_t visits;
} MctsCellResult;

/* The mcts_accel slave lives at LW base + 0x20 (see Qsys integration). */
#define MCTS_BASE          0x0020
#define REG_MCTS_BOARD_LOAD (MCTS_BASE + 0x00)
#define REG_MCTS_KO_LOAD    (MCTS_BASE + 0x04)
#define REG_MCTS_TURN       (MCTS_BASE + 0x08)
#define REG_MCTS_SEED       (MCTS_BASE + 0x0C)
#define REG_MCTS_START      (MCTS_BASE + 0x10)
#define REG_MCTS_STATUS     (MCTS_BASE + 0x14)
#define REG_MCTS_READ_ADDR  (MCTS_BASE + 0x18)
#define REG_MCTS_RESULT     (MCTS_BASE + 0x1C)
#define REG_MCTS_RESET      (MCTS_BASE + 0x20)

#define MCTS_STATUS_DONE    0x1
#define MCTS_STATUS_RUNNING 0x2

/* Returns 0 on success, -1 if the accelerator does not respond. ai.c falls
 * back to ai_mcts_sw_eval if this returns -1. */
int  hw_init_mcts(void);

/* Smoke test: run a known easy capture position; assert that the capture
 * cell dominates win rate. Works against either SW or HW path — set up by
 * ai_init() and called once at startup. Returns 0 on pass, -1 on fail. */
int  hw_mcts_self_test(void);

/* Evaluate one MCTS leaf. Either ai_mcts_sw_eval or ai_mcts_hw_eval is
 * routed here at startup via a function pointer in ai.c. */
void ai_mcts_sw_eval(const BoardState *leaf, uint32_t seed,
                     MctsCellResult out[MCTS_RESULT_N]);
void ai_mcts_hw_eval(const BoardState *leaf, uint32_t seed,
                     MctsCellResult out[MCTS_RESULT_N]);

/* Public dispatcher — calls whichever path ai_init() selected. */
void ai_mcts_dispatch(const BoardState *leaf, uint32_t seed,
                      MctsCellResult out[MCTS_RESULT_N]);

void ai_init(void);

#endif
