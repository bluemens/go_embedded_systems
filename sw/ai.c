/*
 * ai.c — Random / Greedy / MCTS Go AI
 *
 * The MCTS implementation here is intentionally simple:
 *   - Fixed-size node pool (no malloc per node).
 *   - Untried-moves list per node, populated lazily.
 *   - Rollouts use a fast play path: no ko check, just suicide rejection.
 *   - Win-count is from the perspective of the parent node's turn.
 *
 * If MCTS strength is insufficient at 9×9, the next levers (in order):
 *   1. Increase MCTS_SIMULATIONS (currently 200).
 *   2. Add RAVE / progressive bias.
 *   3. Move rollouts to FPGA accelerator (Phase 7b).
 */

#include "ai.h"
#include "ai_hw.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define N BOARD_N

/* ─── PRNG: xorshift32 ────────────────────────────────────────────────────── */

static uint32_t rng_state = 0xDEADBEEFu;

void ai_seed(uint32_t seed)
{
    rng_state = seed ? seed : 0xDEADBEEFu;
}

static inline uint32_t rand32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static inline int rand_below(int n)
{
    return (int)(rand32() % (uint32_t)n);
}

/* ─── Local utility: liberty count + capture (fast, no ko check) ──────────── */

/* Returns liberties of group at (sr, sc). */
static int libs_at(const Stone cells[N][N], int sr, int sc)
{
    Stone c = cells[sr][sc];
    if (c == EMPTY) return -1;
    char visited[N][N];
    memset(visited, 0, sizeof(visited));
    int q[N * N][2]; int qh = 0, qt = 0;
    q[qt][0] = sr; q[qt][1] = sc; qt++;
    visited[sr][sc] = 1;
    int libs = 0;
    while (qh < qt) {
        int r = q[qh][0], cc = q[qh][1]; qh++;
        const int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
        for (int i = 0; i < 4; i++) {
            int nr = r + dr[i], nc = cc + dc[i];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            if (visited[nr][nc]) continue;
            if (cells[nr][nc] == EMPTY) { libs++; visited[nr][nc] = 1; }
            else if (cells[nr][nc] == c) { visited[nr][nc] = 1;
                                           q[qt][0]=nr; q[qt][1]=nc; qt++; }
        }
    }
    return libs;
}

/* Apply move at (r, c) for color, removing dead opponent groups.
 * Caller guarantees the cell is EMPTY and the move is non-suicide.
 * Returns # captured stones. */
static int fast_apply_move(BoardState *b, int row, int col, Stone color)
{
    b->cells[row][col] = color;
    Stone opp = (color == BLACK) ? WHITE : BLACK;
    int total = 0;
    const int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for (int i = 0; i < 4; i++) {
        int nr = row + dr[i], nc = col + dc[i];
        if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
        if (b->cells[nr][nc] != opp) continue;
        if (libs_at((const Stone (*)[N])b->cells, nr, nc) == 0) {
            /* flood-fill remove */
            char visited[N][N];
            memset(visited, 0, sizeof(visited));
            int q[N*N][2]; int qh=0, qt=0;
            q[qt][0]=nr; q[qt][1]=nc; qt++;
            visited[nr][nc] = 1;
            while (qh < qt) {
                int r = q[qh][0], cc = q[qh][1]; qh++;
                if (b->cells[r][cc] == opp) {
                    b->cells[r][cc] = EMPTY;
                    total++;
                    const int ddr[4]={-1,1,0,0}, ddc[4]={0,0,-1,1};
                    for (int j = 0; j < 4; j++) {
                        int rr = r + ddr[j], cc2 = cc + ddc[j];
                        if (rr<0||rr>=N||cc2<0||cc2>=N) continue;
                        if (visited[rr][cc2]) continue;
                        if (b->cells[rr][cc2] == opp) {
                            visited[rr][cc2] = 1;
                            q[qt][0]=rr; q[qt][1]=cc2; qt++;
                        }
                    }
                }
            }
        }
    }
    if (opp == BLACK) b->captured_black += total;
    else              b->captured_white += total;
    return total;
}

/* Is (r, c) a legal move for `color`? Skips ko. */
static int can_play_fast(const BoardState *b, int row, int col, Stone color)
{
    if (row < 0 || row >= N || col < 0 || col >= N) return 0;
    if (b->cells[row][col] != EMPTY) return 0;
    /* Speculative copy */
    BoardState scratch = *b;
    scratch.cells[row][col] = color;
    Stone opp = (color == BLACK) ? WHITE : BLACK;
    int captures = 0;
    const int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
    for (int i = 0; i < 4; i++) {
        int nr = row + dr[i], nc = col + dc[i];
        if (nr<0||nr>=N||nc<0||nc>=N) continue;
        if (scratch.cells[nr][nc] != opp) continue;
        if (libs_at((const Stone (*)[N])scratch.cells, nr, nc) == 0) {
            captures++;
            break;
        }
    }
    if (captures == 0) {
        if (libs_at((const Stone (*)[N])scratch.cells, row, col) == 0)
            return 0;   /* suicide */
    }
    return 1;
}

/* Enumerate legal moves for b->turn into out[], return count. */
static int legal_moves(const BoardState *b, Move *out)
{
    int n = 0;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            if (can_play_fast(b, r, c, b->turn)) {
                out[n].row = r; out[n].col = c; out[n].pass = 0;
                n++;
            }
    return n;
}

/* ─── Level 1: random ────────────────────────────────────────────────────── */

static Move move_random(const BoardState *b)
{
    Move legals[N * N];
    int n = legal_moves(b, legals);
    if (n == 0) {
        Move pass = { 0, 0, 1 };
        return pass;
    }
    return legals[rand_below(n)];
}

/* ─── Level 2: greedy heuristic ──────────────────────────────────────────── */

static Move move_greedy(const BoardState *b)
{
    Move best = { 0, 0, 1 };
    int  best_score = -1;

    Stone color = b->turn;
    Stone opp   = (color == BLACK) ? WHITE : BLACK;

    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (!can_play_fast(b, r, c, color)) continue;

            BoardState scratch = *b;
            int caps = fast_apply_move(&scratch, r, c, color);

            int score = 0;
            score += caps * 10;

            /* Defend an atari group: was an own group at 1 lib that now has >1? */
            const int dr[4]={-1,1,0,0}, dc[4]={0,0,-1,1};
            for (int i = 0; i < 4; i++) {
                int nr = r + dr[i], nc = c + dc[i];
                if (nr<0||nr>=N||nc<0||nc>=N) continue;
                if (b->cells[nr][nc] == color) {
                    int before = libs_at((const Stone(*)[N])b->cells, nr, nc);
                    int after  = libs_at((const Stone(*)[N])scratch.cells, nr, nc);
                    if (before == 1 && after > 1) score += 8;
                }
                /* Reduce opponent libs */
                if (b->cells[nr][nc] == opp && scratch.cells[nr][nc] == opp) {
                    int before = libs_at((const Stone(*)[N])b->cells, nr, nc);
                    int after  = libs_at((const Stone(*)[N])scratch.cells, nr, nc);
                    score += (before - after) * 2;
                }
            }

            /* Center bias: max(0, 4 - chebyshev_dist_to_(4,4)) */
            int cd = abs(r - 4) > abs(c - 4) ? abs(r - 4) : abs(c - 4);
            score += (cd <= 4) ? (4 - cd) : 0;

            /* Random tiebreak for diversity */
            score = score * 16 + (rand32() & 0xF);

            if (score > best_score) {
                best_score = score;
                best.row = r; best.col = c; best.pass = 0;
            }
        }
    }
    return best;
}

/* ─── Phase 9: heat-map publish callback ─────────────────────────────────── */

/* Optional callback installed via ai_set_mcts_heat_callback(). Invoked from
 * inside the tree-MCTS loop every 8 sims (plus once at the end) with a
 * snapshot of root-child win-rates. Used by go_main.c to push live heat
 * values to the FPGA overlay tilemap. */
static MctsHeatCallback g_heat_cb = NULL;

void ai_set_mcts_heat_callback(MctsHeatCallback cb)
{
    g_heat_cb = cb;
}

/* ─── Level 3: serial MCTS (UCT) ─────────────────────────────────────────── */

#define MCTS_POOL       6000
#define MCTS_SIMS       200
#define MCTS_MAX_DEPTH  324    /* 4 × N² */
#define MCTS_CHILDREN_MAX (N * N + 1)   /* 81 cells + pass */
#define UCT_C           1.41421356237

typedef struct MctsNode {
    Move   move;       /* move that led to this node */
    Stone  color;      /* color that just played to reach this node */
    int    visits;
    int    wins;       /* wins for `color` */
    struct MctsNode *parent;
    int    n_children;
    int    n_untried;
    Move   untried[MCTS_CHILDREN_MAX];
    struct MctsNode *children[MCTS_CHILDREN_MAX];
} MctsNode;

static MctsNode mcts_pool[MCTS_POOL];
static int      mcts_pool_n;

static MctsNode *node_new(Move m, Stone color, MctsNode *parent,
                          const BoardState *state)
{
    if (mcts_pool_n >= MCTS_POOL) return NULL;
    MctsNode *n = &mcts_pool[mcts_pool_n++];
    n->move = m;
    n->color = color;
    n->visits = 0;
    n->wins = 0;
    n->parent = parent;
    n->n_children = 0;
    n->n_untried = legal_moves(state, n->untried);
    /* Always allow pass as an option in the tree (otherwise the AI never
     * passes even when surrounded by territory). */
    if (n->n_untried < MCTS_CHILDREN_MAX) {
        Move pass = { 0, 0, 1 };
        n->untried[n->n_untried++] = pass;
    }
    return n;
}

static void apply_move(BoardState *b, Move m)
{
    if (m.pass) {
        b->prev_board_hash = 0; /* irrelevant for fast rollout */
        b->turn = (b->turn == BLACK) ? WHITE : BLACK;
        b->consecutive_passes++;
        if (b->consecutive_passes >= 2) b->game_over = 1;
    } else {
        fast_apply_move(b, m.row, m.col, b->turn);
        b->turn = (b->turn == BLACK) ? WHITE : BLACK;
        b->consecutive_passes = 0;
    }
}

/* Pick child with highest UCB1 score. */
static MctsNode *best_uct_child(MctsNode *node)
{
    MctsNode *best = NULL;
    double    best_v = -1e9;
    double    log_parent = log((double)node->visits + 1.0);
    for (int i = 0; i < node->n_children; i++) {
        MctsNode *ch = node->children[i];
        double exploit = (double)ch->wins / (double)(ch->visits ? ch->visits : 1);
        double explore = UCT_C * sqrt(log_parent / (double)(ch->visits ? ch->visits : 1));
        double v = exploit + explore;
        if (v > best_v) { best_v = v; best = ch; }
    }
    return best;
}

/* Random rollout from `state` to terminal (2 passes or move limit).
 * Returns the winning color (BLACK or WHITE). */
static Stone rollout(BoardState state)
{
    int moves = 0;
    while (!state.game_over && moves < MCTS_MAX_DEPTH) {
        /* Pick a random legal move; if none, pass. */
        Move legals[N * N];
        int  n = legal_moves(&state, legals);
        Move m;
        if (n == 0) { m.row = 0; m.col = 0; m.pass = 1; }
        else        m = legals[rand_below(n)];
        apply_move(&state, m);
        moves++;
    }
    int blk, wht;
    board_score(&state, &blk, &wht);
    return (blk > wht + 5.5) ? BLACK : WHITE;
}

/* Walk root->children, fill the two 82-slot tables (81 cells + pass), and
 * invoke the installed heat callback. Used to publish live MCTS progress
 * during tree-MCTS thinking. Idx >= 82 entries (shouldn't occur) are
 * silently dropped. */
static void publish_heat(const MctsNode *root)
{
    if (!g_heat_cb) return;
    float wr[82];
    int   v[82];
    for (int i = 0; i < 82; i++) { wr[i] = 0.0f; v[i] = 0; }
    for (int i = 0; i < root->n_children; i++) {
        const MctsNode *ch = root->children[i];
        int idx = ch->move.pass ? 81 : (ch->move.row * 9 + ch->move.col);
        if (idx < 0 || idx >= 82) continue;
        v[idx]  = ch->visits;
        wr[idx] = ch->visits ? (float)ch->wins / (float)ch->visits : 0.0f;
    }
    g_heat_cb(wr, v);
}

static Move move_mcts(const BoardState *b)
{
    mcts_pool_n = 0;
    Stone root_color = (b->turn == BLACK) ? WHITE : BLACK;
    Move root_move = { 0, 0, 0 };
    MctsNode *root = node_new(root_move, root_color, NULL, b);
    if (!root) {
        /* Pool exhausted before root; fall back to greedy. */
        return move_greedy(b);
    }

    for (int sim = 0; sim < MCTS_SIMS; sim++) {
        MctsNode *node = root;
        BoardState state = *b;

        /* Selection: descend while node is fully expanded and has children. */
        while (node->n_untried == 0 && node->n_children > 0) {
            MctsNode *ch = best_uct_child(node);
            if (!ch) break;
            apply_move(&state, ch->move);
            node = ch;
        }

        /* Expansion */
        if (node->n_untried > 0) {
            int idx = rand_below(node->n_untried);
            Move m = node->untried[idx];
            node->untried[idx] = node->untried[--node->n_untried];
            apply_move(&state, m);
            MctsNode *child = node_new(m, state.turn == BLACK ? WHITE : BLACK,
                                       node, &state);
            if (child) {
                node->children[node->n_children++] = child;
                node = child;
            }
        }

        /* Simulation */
        Stone winner = rollout(state);

        /* Backpropagation */
        for (MctsNode *p = node; p != NULL; p = p->parent) {
            p->visits++;
            if (p->color == winner) p->wins++;
        }

        /* Phase 9: publish heat every 8 sims (25 snapshots over 200 sims). */
        if ((sim & 7) == 7) publish_heat(root);
    }

    /* Final publish so the chosen cell goes hot before the caller clears
     * the overlay. */
    publish_heat(root);

    /* Pick child with highest visit count. */
    MctsNode *best = NULL;
    int best_visits = -1;
    for (int i = 0; i < root->n_children; i++) {
        if (root->children[i]->visits > best_visits) {
            best_visits = root->children[i]->visits;
            best = root->children[i];
        }
    }
    if (!best) {
        Move pass = { 0, 0, 1 };
        return pass;
    }
    return best->move;
}

/* ─── Public dispatch ────────────────────────────────────────────────────── */

/* Forward declaration: filled in below the leaf-eval implementation. */
static int g_use_hw_mcts = 0;
static Move move_mcts_flat_hw(const BoardState *b);

/* ─── Phase 9: demo-facing toggle accessors over g_use_hw_mcts ───────────── */

void ai_toggle_backend(void)
{
    g_use_hw_mcts = !g_use_hw_mcts;
}

int ai_backend_is_hw(void)
{
    return g_use_hw_mcts;
}

const char *ai_backend_label(void)
{
    return g_use_hw_mcts ? "HW" : "SW";
}

Move ai_get_move(const BoardState *b, AiLevel level)
{
    switch (level) {
    case AI_RANDOM: return move_random(b);
    case AI_GREEDY: return move_greedy(b);
    case AI_MCTS:   return g_use_hw_mcts ? move_mcts_flat_hw(b) : move_mcts(b);
    }
    Move pass = { 0, 0, 1 };
    return pass;
}

/* ─── Leaf-eval entrypoint (mirror of HW path) ──────────────────────────── */

/* One rollout that records the first move played; used by ai_mcts_sw_eval to
 * credit per-cell wins/visits. Cell index 81 == pass. */
static Stone rollout_first_move(BoardState state, int *first_cell)
{
    int moves = 0;
    *first_cell = 81;
    while (!state.game_over && moves < MCTS_MAX_DEPTH) {
        Move legals[N * N];
        int n = legal_moves(&state, legals);
        Move m;
        if (n == 0) {
            m.row = 0; m.col = 0; m.pass = 1;
        } else {
            m = legals[rand_below(n)];
        }
        if (moves == 0) {
            *first_cell = m.pass ? 81 : (m.row * N + m.col);
        }
        apply_move(&state, m);
        moves++;
    }
    int blk, wht;
    board_score(&state, &blk, &wht);
    return (blk > wht + 5) ? BLACK : WHITE;   /* 5.5 komi, white wins ties */
}

void ai_mcts_sw_eval(const BoardState *leaf, uint32_t seed,
                     MctsCellResult out[MCTS_RESULT_N])
{
    ai_seed(seed);
    for (int i = 0; i < MCTS_RESULT_N; i++) {
        out[i].wins = 0;
        out[i].visits = 0;
    }
    Stone root_turn = leaf->turn;
    for (int s = 0; s < MCTS_SIMS; s++) {
        int first_cell = 81;
        Stone winner = rollout_first_move(*leaf, &first_cell);
        out[first_cell].visits++;
        if (winner == root_turn) out[first_cell].wins++;
    }
}

/* Dispatch via function pointer set by ai_init(). */
typedef void (*mcts_eval_fn_t)(const BoardState *, uint32_t,
                               MctsCellResult[MCTS_RESULT_N]);
static mcts_eval_fn_t g_mcts_eval = ai_mcts_sw_eval;

void ai_mcts_dispatch(const BoardState *leaf, uint32_t seed,
                      MctsCellResult out[MCTS_RESULT_N])
{
    g_mcts_eval(leaf, seed, out);
}

int hw_mcts_self_test(void)
{
    /* Sanity test: proves the eval path is functioning without depending on
     * single-move statistical dominance. Uniform-random 9x9 rollouts have
     * ~10% per-cell noise at 200 sims, so we cannot reliably assert "the
     * capture move dominates" without a deeper rollout policy. Real
     * correctness comes from the A/B harness across many leaves
     * (`go_self_test ab N`) — this is just the wedged-FSM detector. */
    BoardState b;
    board_init(&b);
    b.turn = BLACK;
    b.cells[0][0] = BLACK;
    b.cells[0][1] = WHITE;
    b.cells[1][1] = BLACK;

    MctsCellResult out[MCTS_RESULT_N];
    ai_mcts_dispatch(&b, 0xDEADBEEF, out);

    int total_visits = 0, total_wins = 0, distinct_cells = 0;
    for (int i = 0; i < MCTS_RESULT_N; i++) {
        total_visits += out[i].visits;
        total_wins   += out[i].wins;
        if (out[i].visits > 0) distinct_cells++;
    }

    if (total_visits != MCTS_SIMS) {
        fprintf(stderr, "self_test: visits=%d != MCTS_SIMS=%d\n",
                total_visits, MCTS_SIMS);
        return -1;
    }
    if (distinct_cells < 5) {
        fprintf(stderr,
                "self_test: only %d distinct cells visited (FSM wedged?)\n",
                distinct_cells);
        return -1;
    }
    if (total_wins < MCTS_SIMS / 20 || total_wins > MCTS_SIMS * 19 / 20) {
        fprintf(stderr,
                "self_test: wins=%d out of plausible [5%%, 95%%] band\n",
                total_wins);
        return -1;
    }
    return 0;
}

void ai_init(void)
{
    if (hw_init_mcts() == 0) {
        g_mcts_eval = ai_mcts_hw_eval;
        g_use_hw_mcts = 1;
    } else {
        g_mcts_eval = ai_mcts_sw_eval;
        g_use_hw_mcts = 0;
    }
}

/* Flat HW-MCTS: one dispatcher call on the root, pick the cell with the
 * most visits. Strictly weaker than the tree-based SW move_mcts(), but
 * orders of magnitude faster — and the entire point of the HW path. */
static Move move_mcts_flat_hw(const BoardState *b)
{
    static uint32_t hw_call_counter = 0;
    uint32_t seed = (uint32_t)rand32() ^ (++hw_call_counter * 0x9E3779B9u);
    MctsCellResult out[MCTS_RESULT_N];
    ai_mcts_dispatch(b, seed, out);

    int best_idx = 81;
    int best_visits = -1;
    for (int i = 0; i < MCTS_RESULT_N; i++) {
        if ((int)out[i].visits > best_visits) {
            best_visits = (int)out[i].visits;
            best_idx = i;
        }
    }

    /* Publish a one-shot heat snapshot from the HW (or HW-fallback-to-SW)
     * result. The SW tree-MCTS publishes incrementally inside its sim loop;
     * the HW path runs in one shot and only has data at the end, so this is
     * a single post-hoc publish — enough for the demo's "after" heat colors. */
    if (g_heat_cb) {
        float wr[82];
        int   v[82];
        for (int i = 0; i < 82; i++) {
            v[i]  = out[i].visits;
            wr[i] = out[i].visits ? (float)out[i].wins / (float)out[i].visits
                                  : 0.0f;
        }
        g_heat_cb(wr, v);
    }

    Move m;
    if (best_idx == 81 || best_visits == 0) {
        m.row = 0; m.col = 0; m.pass = 1;
    } else {
        m.row = best_idx / N;
        m.col = best_idx % N;
        m.pass = 0;
    }
    return m;
}
