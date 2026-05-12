/*
 * ai_mcts_hw.c — userspace driver for the FPGA MCTS rollout accelerator.
 *
 * Protocol per design-doc §7.5.10:
 *   1. Stream 162 board bits via REG_MCTS_BOARD_LOAD
 *   2. Set REG_MCTS_TURN, REG_MCTS_SEED
 *   3. Pulse REG_MCTS_START
 *   4. Poll REG_MCTS_STATUS until done bit
 *   5. Read 82 (wins, visits) pairs via REG_MCTS_READ_ADDR + REG_MCTS_RESULT
 */
#include "ai_hw.h"

#include <stdio.h>
#include <unistd.h>

#include "board.h"
#include "hw_iface.h"

int hw_init_mcts(void)
{
    if (hw_init() != 0) return -1;
    hw_write(REG_MCTS_RESET, 1);
    hw_write(REG_MCTS_RESET, 0);
    hw_write(REG_MCTS_SEED, 0xDEADBEEF);
    uint32_t s = hw_read(REG_MCTS_STATUS);
    /* After reset, running bit must be clear. */
    if (s & MCTS_STATUS_RUNNING) return -1;
    return 0;
}

void ai_mcts_hw_eval(const BoardState *leaf, uint32_t seed,
                     MctsCellResult out[MCTS_RESULT_N])
{
    /* Stream the board: 81 black-plane bits, then 81 white-plane bits. */
    for (int i = 0; i < 81; i++) {
        int row = i / 9, col = i % 9;
        Stone s = leaf->cells[row][col];
        uint32_t pkt_b = (0u << 16) | ((uint32_t)i << 8) | (s == BLACK ? 1u : 0u);
        uint32_t pkt_w = (1u << 16) | ((uint32_t)i << 8) | (s == WHITE ? 1u : 0u);
        hw_write(REG_MCTS_BOARD_LOAD, pkt_b);
        hw_write(REG_MCTS_BOARD_LOAD, pkt_w);
    }

    hw_write(REG_MCTS_TURN, leaf->turn == WHITE ? 1u : 0u);
    hw_write(REG_MCTS_SEED, seed);
    hw_write(REG_MCTS_START, 1u);

    while (!(hw_read(REG_MCTS_STATUS) & MCTS_STATUS_DONE)) {
        usleep(100);
    }

    for (int i = 0; i < MCTS_RESULT_N; i++) {
        hw_write(REG_MCTS_READ_ADDR, (uint32_t)i);
        uint32_t r = hw_read(REG_MCTS_RESULT);
        out[i].wins   = (uint16_t)(r & 0xFFFF);
        out[i].visits = (uint16_t)(r >> 16);
    }
}
