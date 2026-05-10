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

#include <math.h>
#include <string.h>
#include <stdlib.h>

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
    }

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

Move ai_get_move(const BoardState *b, AiLevel level)
{
    switch (level) {
    case AI_RANDOM: return move_random(b);
    case AI_GREEDY: return move_greedy(b);
    case AI_MCTS:   return move_mcts(b);
    }
    Move pass = { 0, 0, 1 };
    return pass;
}
