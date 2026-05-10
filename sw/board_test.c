/*
 * board_test.c — host-runnable sanity tests for board.c
 *
 * No hardware dependencies, so this can be compiled and run on a Mac/Linux
 * dev laptop. Useful for catching rule-engine bugs without booting the FPGA.
 *
 * Build:   make board_test       (or: cc -O2 -Wall board_test.c board.c -o board_test)
 * Run:     ./board_test
 *
 * The tests are deliberately simple — they exercise each rule once. They are
 * NOT a comprehensive Go test suite. If the basics here pass, we trust the
 * algorithm; if a real game shows weird behavior, add a regression test here.
 */

#include "board.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) do {                                       \
    if (!(cond)) {                                                  \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__);   \
        failures++;                                                 \
    }                                                               \
} while (0)

static void test_init(void)
{
    printf("test_init\n");
    BoardState b;
    board_init(&b);
    for (int r = 0; r < BOARD_N; r++)
        for (int c = 0; c < BOARD_N; c++)
            CHECK(b.cells[r][c] == EMPTY, "fresh board not empty");
    CHECK(b.turn == BLACK, "Black moves first");
    CHECK(b.captured_black == 0 && b.captured_white == 0, "no captures yet");
    CHECK(!b.game_over, "game not over");
}

static void test_basic_placement(void)
{
    printf("test_basic_placement\n");
    BoardState b;
    board_init(&b);

    CHECK(board_place(&b, 4, 4) == MOVE_OK, "place at tengen");
    CHECK(b.cells[4][4] == BLACK, "tengen is black");
    CHECK(b.turn == WHITE, "turn toggled to white");

    CHECK(board_place(&b, 4, 4) == MOVE_ILLEGAL_OCCUPIED, "can't replay");
    CHECK(board_place(&b, 5, 5) == MOVE_OK, "white plays elsewhere");
    CHECK(b.cells[5][5] == WHITE, "white placed");
}

/*
 * test_simple_capture: a single isolated black stone in the corner gets
 * surrounded and captured.
 *
 *    . W .            . W .
 *    W B .   ──W──▶   W . .
 *    . . .            . . .
 *
 * Sequence: B(0,0) W(0,1) B(0,2) W(1,0) B(1,1) W(2,0) → captures (1,0)? No.
 * Simpler: place B at (0,0). W at (0,1). W at (1,0). White's last move
 * completes the surround → black at (0,0) has 0 liberties → captured.
 */
static void test_simple_capture(void)
{
    printf("test_simple_capture\n");
    BoardState b;
    board_init(&b);

    /* Black plays in the corner */
    CHECK(board_place(&b, 0, 0) == MOVE_OK, "B(0,0)");
    /* White plays at (0,1) */
    CHECK(board_place(&b, 0, 1) == MOVE_OK, "W(0,1)");
    /* Black plays elsewhere (give up the corner stone) */
    CHECK(board_place(&b, 4, 4) == MOVE_OK, "B(4,4)");
    /* White seals (1,0) — should capture B(0,0) */
    CHECK(board_place(&b, 1, 0) == MOVE_OK, "W(1,0) seals corner");
    CHECK(b.cells[0][0] == EMPTY, "B(0,0) captured");
    CHECK(b.captured_black == 1, "1 black captured");
}

/*
 * test_suicide: black tries to place into a fully-surrounded empty point
 * with no captures available. Should be rejected.
 *
 *    W .  ←  rows 0..1, cols 0..1
 *    . W
 *
 *  Wait that's not surrounded. Try:
 *
 *    . W .    Black to play at (1,0)? (1,0) has neighbors (0,0)=. (2,0)=W (1,1)=W
 *    W . W    so (1,0) has 1 liberty (0,0). Not suicide.
 *    . W .
 *
 * Need to fully surround. Single point at (1,1):
 *    . W .
 *    W . W
 *    . W .
 *
 * Black plays at (1,1): neighbors are (0,1)=W (2,1)=W (1,0)=W (1,2)=W. 0 libs.
 * No captures (those white groups all have other liberties). → SUICIDE.
 */
static void test_suicide(void)
{
    printf("test_suicide\n");
    BoardState b;
    board_init(&b);

    /* Build the W pattern with intervening B moves elsewhere */
    CHECK(board_place(&b, 0, 1) == MOVE_OK, "B(0,1) placeholder"); /* black's turn */
    CHECK(board_place(&b, 0, 1) == MOVE_ILLEGAL_OCCUPIED, "can't replay");

    /* Reset and build the suicide setup directly. We'll cheat by manipulating
     * cells directly after init — this is a test, not real gameplay. */
    board_init(&b);
    b.cells[0][1] = WHITE;
    b.cells[2][1] = WHITE;
    b.cells[1][0] = WHITE;
    b.cells[1][2] = WHITE;
    /* Give the white stones distant friends so they have liberties */
    b.cells[0][2] = WHITE; /* (0,1) has lib at (0,0) */
    b.cells[2][2] = WHITE; /* etc */
    b.cells[2][0] = WHITE;
    b.cells[0][0] = WHITE; /* (1,0) has libs via (0,0)=W friend */
    /* Black to play at (1,1): own libs 0, no captures (white groups have libs) */
    CHECK(board_place(&b, 1, 1) == MOVE_ILLEGAL_SUICIDE, "(1,1) is suicide");
}

/*
 * test_ko: classic ko shape.
 *
 *   . W B .         . W B .
 *   W . W B    ──▶  W B . B    ← B captures W at (1,1)
 *   . W B .         . W B .
 *
 * White's reply at (1,1) would recreate the original — that's ko, illegal.
 */
static void test_ko(void)
{
    printf("test_ko\n");
    BoardState b;
    board_init(&b);

    /* Set up the position via direct cell writes (we control the test). */
    b.cells[0][1] = WHITE; b.cells[0][2] = BLACK;
    b.cells[1][0] = WHITE; b.cells[1][2] = WHITE; b.cells[1][3] = BLACK;
    b.cells[2][1] = WHITE; b.cells[2][2] = BLACK;
    /* After black plays (1,1), white at (1,2) has 0 libs and is captured.
     * Then white tries (1,2) which would recapture (1,1) — KO. */

    /* Capture (snapshot pre-move hash for ko comparison): */
    b.turn = BLACK;
    /* Manually emulate "the position before B's move" being the ko-target */
    extern void board_init(BoardState*);  /* re-declare to keep compiler quiet */
    /* For ko detection to work, prev_board_hash must equal the position
     * AFTER the white capture is undone — i.e., the position from before
     * the upcoming sequence. board_place will set prev_board_hash before
     * applying its mutation, so we just need to call it. */

    CHECK(board_place(&b, 1, 1) == MOVE_OK, "B(1,1) captures W(1,2)");
    CHECK(b.cells[1][2] == EMPTY, "W(1,2) captured");
    CHECK(b.captured_white == 1, "1 white captured");
    CHECK(board_place(&b, 1, 2) == MOVE_ILLEGAL_KO, "W(1,2) recapture is ko");
}

static void test_two_passes_end_game(void)
{
    printf("test_two_passes_end_game\n");
    BoardState b;
    board_init(&b);
    CHECK(!b.game_over, "fresh game not over");
    board_pass(&b);
    CHECK(!b.game_over, "one pass: not over");
    board_pass(&b);
    CHECK(b.game_over, "two passes: game over");
}

static void test_simple_score(void)
{
    printf("test_simple_score\n");
    BoardState b;
    board_init(&b);
    /* Tiny territory: black walls off bottom-left 2×2 corner. */
    b.cells[0][0] = BLACK; b.cells[0][1] = BLACK; b.cells[0][2] = BLACK;
    b.cells[1][2] = BLACK;
    b.cells[2][0] = BLACK; b.cells[2][1] = BLACK; b.cells[2][2] = BLACK;
    /* White takes bottom-right corner */
    b.cells[6][6] = WHITE; b.cells[6][7] = WHITE; b.cells[6][8] = WHITE;
    b.cells[7][6] = WHITE;
    b.cells[8][6] = WHITE; b.cells[8][7] = WHITE; b.cells[8][8] = WHITE;
    /* Cell (1,0) and (1,1) should be black territory: bordered only by black.
     * Cell (7,7) and (7,8) should be white territory. */

    int blk, wht;
    board_score(&b, &blk, &wht);
    /* 7 black stones + 2 territory = 9; 7 white stones + 2 territory = 9. */
    /* The middle "open" area is bordered by both colors (or by edges only on
     * a 9x9 board? edges aren't a color), so with our flood algorithm it
     * borders both → neutral. So expected: 9, 9 (or close). */
    /* Actually most of the empty space borders both → neutral. So we expect
     * exactly 7+2=9 and 7+2=9. */
    CHECK(blk == 9, "black scores 9 (7 stones + 2 territory)");
    CHECK(wht == 9, "white scores 9 (7 stones + 2 territory)");
}

int main(void)
{
    test_init();
    test_basic_placement();
    test_simple_capture();
    test_suicide();
    test_ko();
    test_two_passes_end_game();
    test_simple_score();

    if (failures == 0) {
        printf("\nAll tests passed.\n");
        return 0;
    }
    printf("\n%d failure(s).\n", failures);
    return 1;
}
