/*
 * board.h — Go rule engine API
 *
 * Pure C, no hardware dependencies — board.c can be unit-tested on any host.
 * The 9x9 board state, move validation (occupancy / suicide / ko), capture
 * detection, scoring, and Zobrist hashing all live here.
 *
 * Reference: design-document.md §7.
 */

#ifndef _BOARD_H
#define _BOARD_H

#include <stdint.h>

#define BOARD_N 9

typedef enum {
    EMPTY = 0,
    BLACK = 1,
    WHITE = 2,
} Stone;

typedef enum {
    MOVE_OK = 0,
    MOVE_ILLEGAL_OCCUPIED,
    MOVE_ILLEGAL_SUICIDE,
    MOVE_ILLEGAL_KO,
} MoveResult;

typedef struct {
    Stone    cells[BOARD_N][BOARD_N];   /* [row][col]                          */
    Stone    turn;                      /* whose turn it is (BLACK or WHITE)   */
    int      captured_black;            /* count of black stones captured by W */
    int      captured_white;            /* count of white stones captured by B */
    uint64_t prev_board_hash;           /* Zobrist hash of position before our  *
                                         * last move — used to detect simple   *
                                         * ko (immediate capture-back).        */
    int      consecutive_passes;        /* 2 = game over                       */
    int      game_over;                 /* set when consecutive_passes >= 2    */
} BoardState;

/* Initialize an empty board with Black to move and a fresh Zobrist table.
 * Calling this multiple times resets state but seeds the Zobrist table only
 * once (so prior hashes remain valid across multiple board_init calls — useful
 * for tests). */
void board_init(BoardState *b);

/* Place a stone for b->turn at (row, col). On MOVE_OK:
 *   - the stone is placed,
 *   - any opponent groups with zero liberties are removed,
 *   - captured_black/white are updated,
 *   - prev_board_hash is updated to hold the pre-move hash,
 *   - turn is toggled to the opponent,
 *   - consecutive_passes reset to 0.
 * On any failure: state is unchanged. */
MoveResult board_place(BoardState *b, int row, int col);

/* Pass the turn. Always legal. Increments consecutive_passes; game_over
 * becomes true when it reaches 2. */
void board_pass(BoardState *b);

/* Chinese-style area score: stones on board + territory enclosed solely by
 * one color. Komi (5.5 for white) is NOT applied here; the caller adds it.
 * Writes integer point counts to *black_pts and *white_pts. */
void board_score(const BoardState *b, int *black_pts, int *white_pts);

#endif /* _BOARD_H */
