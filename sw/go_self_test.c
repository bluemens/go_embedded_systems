/*
 * go_self_test — first-boot bring-up smoke test for the MCTS accelerator.
 *
 * Usage:
 *   ./go_self_test            # full sequence: probe + self_test + a/b
 *   ./go_self_test probe      # just hw_init_mcts()
 *   ./go_self_test selftest   # just the easy-capture position
 *   ./go_self_test ab N       # SW vs HW agreement test over N seeds
 *   ./go_self_test --no-hw    # force SW path (skip hw_init_mcts)
 *
 * Exit code 0 = pass, 1 = fail. No USB, no display.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "ai_hw.h"
#include "board.h"

static int do_probe(void)
{
    if (hw_init_mcts() != 0) {
        fprintf(stderr, "probe: hw_init_mcts() failed\n");
        return 1;
    }
    printf("probe: OK\n");
    return 0;
}

static int do_self_test(void)
{
    ai_init();
    if (hw_mcts_self_test() != 0) {
        fprintf(stderr, "self_test: FAIL\n");
        return 1;
    }
    printf("self_test: OK\n");
    return 0;
}

/* Run N independent leaves through SW eval and HW eval; report per-cell
 * mean absolute error in win rate. Used at end of Step 10 of the plan to
 * gain statistical confidence that the HW path agrees with SW. */
static int do_ab(int n)
{
    int hw_ok = (hw_init_mcts() == 0);
    if (!hw_ok) {
        fprintf(stderr, "ab: HW not available; cannot run comparison\n");
        return 1;
    }

    double total_abs_err = 0.0;
    int    samples = 0;

    for (int t = 0; t < n; t++) {
        /* Each iteration: random sparse board, both paths, same seed. */
        BoardState b;
        board_init(&b);
        uint32_t seed = 0xC0FFEE00u + (uint32_t)t;
        srand(seed);
        int n_stones = rand() % 20;
        for (int s = 0; s < n_stones; s++) {
            int row = rand() % BOARD_N;
            int col = rand() % BOARD_N;
            if (b.cells[row][col] == EMPTY)
                b.cells[row][col] = (rand() & 1) ? BLACK : WHITE;
        }
        b.turn = (rand() & 1) ? BLACK : WHITE;

        MctsCellResult sw[MCTS_RESULT_N], hw[MCTS_RESULT_N];
        ai_mcts_sw_eval(&b, seed, sw);
        ai_mcts_hw_eval(&b, seed, hw);
        for (int i = 0; i < MCTS_RESULT_N; i++) {
            int sv = sw[i].visits, hv = hw[i].visits;
            if (sv == 0 && hv == 0) continue;
            double swr = sv ? (double)sw[i].wins / (double)sv : 0.0;
            double hwr = hv ? (double)hw[i].wins / (double)hv : 0.0;
            total_abs_err += fabs(swr - hwr);
            samples++;
        }
    }

    double mae = samples ? total_abs_err / samples : 0.0;
    printf("ab: leaves=%d samples=%d MAE=%.4f\n", n, samples, mae);
    return (mae < 0.10) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "probe") == 0) return do_probe();
    if (argc > 1 && strcmp(argv[1], "selftest") == 0) return do_self_test();
    if (argc > 1 && strcmp(argv[1], "ab") == 0) {
        int n = (argc > 2) ? atoi(argv[2]) : 1000;
        return do_ab(n);
    }
    /* Default: full sequence */
    int rc = 0;
    rc |= do_probe();
    rc |= do_self_test();
    rc |= do_ab(100);
    return rc;
}
