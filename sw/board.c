/*
 * board.c — Go rule engine implementation
 *
 * Pure C, no hardware dependencies. Algorithms straight from
 * design-document.md §7. Liberty counting is a queue-based BFS over
 * 4-connected neighbors. Captures are detected by counting liberties of each
 * opponent group adjacent to a placed stone after speculative placement.
 * Ko detection uses a 64-bit Zobrist hash compared to the previous board
 * position; this catches simple (one-move) ko but NOT positional super-ko.
 */

#include "board.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#define N BOARD_N

/* ─── Zobrist hash table — initialized once on first board_init() ─────────── */

static uint64_t zob_table[N][N][3];   /* index: row, col, stone(0..2) */
static int      zob_initialized = 0;

static uint64_t rand64(void)
{
    /* Combine two rand() calls; rand() returns up to RAND_MAX which is at
     * least 2^15 - 1. Quality is fine for hash-table seeds. */
    uint64_t hi = (uint64_t)rand();
    uint64_t lo = (uint64_t)rand();
    return (hi << 32) ^ ((uint64_t)rand() << 16) ^ lo;
}

static void zob_init_once(void)
{
    if (zob_initialized) return;
    /* Deterministic seed so that test runs are reproducible. The actual hash
     * values are irrelevant as long as they're well-distributed. */
    srand(0xC0FFEE);
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            for (int s = 0; s < 3; s++)
                zob_table[r][c][s] = rand64();
    zob_initialized = 1;
}

static uint64_t zob_hash(const Stone cells[N][N])
{
    uint64_t h = 0;
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++)
            h ^= zob_table[r][c][cells[r][c]];
    return h;
}

/* ─── BFS liberty count + group collection ──────────────────────────────── */

/* Walks the connected group containing (sr, sc); fills group[] with up to
 * cap members; counts unique adjacent EMPTY cells. Returns liberty count.
 * If group is NULL, only the count is computed. */
static int liberties_and_group(const Stone cells[N][N], int sr, int sc,
                               int (*group)[2], int cap, int *group_size)
{
    Stone color = cells[sr][sc];
    if (color == EMPTY) { if (group_size) *group_size = 0; return -1; }

    /* visited tracks both group cells and counted liberty cells, so we don't
     * double-count a liberty bordering multiple stones in the group. */
    char visited[N][N];
    memset(visited, 0, sizeof(visited));

    int q[N * N][2];
    int qh = 0, qt = 0;
    q[qt][0] = sr; q[qt][1] = sc; qt++;
    visited[sr][sc] = 1;

    int liberties = 0;
    int g_n = 0;

    while (qh < qt) {
        int r = q[qh][0], c = q[qh][1]; qh++;
        if (group && g_n < cap) {
            group[g_n][0] = r;
            group[g_n][1] = c;
        }
        g_n++;

        const int dr[4] = {-1, 1,  0, 0};
        const int dc[4] = { 0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            if (visited[nr][nc]) continue;
            if (cells[nr][nc] == EMPTY) {
                liberties++;
                visited[nr][nc] = 1;
            } else if (cells[nr][nc] == color) {
                visited[nr][nc] = 1;
                q[qt][0] = nr; q[qt][1] = nc; qt++;
            }
        }
    }

    if (group_size) *group_size = g_n;
    return liberties;
}

/* Capture every opponent group adjacent to (r, c) that has 0 liberties.
 * Returns the count of stones removed. Updates b->captured_*. */
static int process_captures(BoardState *b, int r, int c)
{
    Stone placed = b->cells[r][c];
    Stone opp    = (placed == BLACK) ? WHITE : BLACK;
    int total = 0;

    const int dr[4] = {-1, 1,  0, 0};
    const int dc[4] = { 0, 0, -1, 1};
    for (int i = 0; i < 4; i++) {
        int nr = r + dr[i], nc = c + dc[i];
        if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
        if (b->cells[nr][nc] != opp) continue;

        int group[N * N][2];
        int g_n = 0;
        int libs = liberties_and_group(
            (const Stone (*)[N])b->cells, nr, nc, group, N * N, &g_n);
        if (libs == 0) {
            for (int j = 0; j < g_n; j++)
                b->cells[group[j][0]][group[j][1]] = EMPTY;
            if (opp == BLACK) b->captured_black += g_n;
            else              b->captured_white += g_n;
            total += g_n;
        }
    }
    return total;
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

void board_init(BoardState *b)
{
    zob_init_once();
    memset(b, 0, sizeof(*b));
    b->turn = BLACK;
    b->prev_board_hash = zob_hash((const Stone (*)[N])b->cells);
}

MoveResult board_place(BoardState *b, int row, int col)
{
    if (row < 0 || row >= N || col < 0 || col >= N) return MOVE_ILLEGAL_OCCUPIED;
    if (b->cells[row][col] != EMPTY) return MOVE_ILLEGAL_OCCUPIED;

    /* Speculatively place on a scratch board */
    Stone scratch[N][N];
    memcpy(scratch, b->cells, sizeof(scratch));
    scratch[row][col] = b->turn;

    /* Capture opponent groups (mutates scratch) */
    BoardState tmp = *b;
    memcpy(tmp.cells, scratch, sizeof(scratch));
    int captures = process_captures(&tmp, row, col);

    /* Suicide: own group has no liberties AND no captures occurred */
    int g_n = 0;
    int own_libs = liberties_and_group(
        (const Stone (*)[N])tmp.cells, row, col, NULL, 0, &g_n);
    if (own_libs == 0 && captures == 0)
        return MOVE_ILLEGAL_SUICIDE;

    /* Ko: resulting position == previous position */
    uint64_t new_hash = zob_hash((const Stone (*)[N])tmp.cells);
    if (new_hash == b->prev_board_hash)
        return MOVE_ILLEGAL_KO;

    /* Commit. Save current hash for next move's ko check. */
    b->prev_board_hash = zob_hash((const Stone (*)[N])b->cells);
    memcpy(b->cells, tmp.cells, sizeof(tmp.cells));
    b->captured_black = tmp.captured_black;
    b->captured_white = tmp.captured_white;
    b->turn = (b->turn == BLACK) ? WHITE : BLACK;
    b->consecutive_passes = 0;
    return MOVE_OK;
}

void board_pass(BoardState *b)
{
    b->prev_board_hash = zob_hash((const Stone (*)[N])b->cells);
    b->turn = (b->turn == BLACK) ? WHITE : BLACK;
    b->consecutive_passes++;
    if (b->consecutive_passes >= 2)
        b->game_over = 1;
}

/* Flood-fill an empty region; record colors that border it. */
static void flood_empty_region(const Stone cells[N][N], int sr, int sc,
                               char visited[N][N],
                               int *region_size, int *seen_black, int *seen_white)
{
    int q[N * N][2];
    int qh = 0, qt = 0;
    q[qt][0] = sr; q[qt][1] = sc; qt++;
    visited[sr][sc] = 1;
    *region_size = 0;
    *seen_black = 0;
    *seen_white = 0;

    while (qh < qt) {
        int r = q[qh][0], c = q[qh][1]; qh++;
        (*region_size)++;

        const int dr[4] = {-1, 1,  0, 0};
        const int dc[4] = { 0, 0, -1, 1};
        for (int i = 0; i < 4; i++) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= N || nc < 0 || nc >= N) continue;
            if (cells[nr][nc] == EMPTY) {
                if (!visited[nr][nc]) {
                    visited[nr][nc] = 1;
                    q[qt][0] = nr; q[qt][1] = nc; qt++;
                }
            } else if (cells[nr][nc] == BLACK) {
                *seen_black = 1;
            } else if (cells[nr][nc] == WHITE) {
                *seen_white = 1;
            }
        }
    }
}

void board_score(const BoardState *b, int *black_pts, int *white_pts)
{
    int blk = 0, wht = 0;

    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            if (b->cells[r][c] == BLACK) blk++;
            else if (b->cells[r][c] == WHITE) wht++;
        }

    char visited[N][N];
    memset(visited, 0, sizeof(visited));
    for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
            if (b->cells[r][c] != EMPTY || visited[r][c]) continue;
            int sz, sb, sw;
            flood_empty_region((const Stone (*)[N])b->cells, r, c, visited,
                               &sz, &sb, &sw);
            if (sb && !sw)      blk += sz;
            else if (sw && !sb) wht += sz;
            /* contested or fully surrounded by neither (shouldn't happen on
             * a complete board): neutral, no points. */
        }

    *black_pts = blk;
    *white_pts = wht;
}
