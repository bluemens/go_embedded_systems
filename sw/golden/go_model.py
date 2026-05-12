"""Pure-Python 9x9 Go reference model — bit-exact mirror of the Verilog.

Reads/writes use the same bit ordering and shift semantics as
hw/ip/mcts_accel/mcts_accel_pkg.sv:
    cell_idx = row * 9 + col
    bit cell_idx of an 81-bit integer
    row 0 col 0 = bit 0 (LSB), row 8 col 8 = bit 80 (MSB)

Provides:
    - flood_fill(seed_bit, propagate_mask, liberty_mask) -> (group, size, has_lib)
    - rollout(black, white, turn, ko_b, ko_w, seed) -> per-cell wins/visits
    - constants ON_BOARD_MASK, COL0_MASK, COL8_MASK, KOMI_INT
"""

from __future__ import annotations

# === Bitboard constants (must match mcts_accel_pkg.sv exactly) =============

ON_BOARD_MASK = (1 << 81) - 1
COL0_MASK = sum(1 << (r * 9 + 0) for r in range(9))
COL8_MASK = sum(1 << (r * 9 + 8) for r in range(9))
KOMI_INT = 5  # 5.5 komi with white-wins-ties tiebreak


def shift_n(x: int) -> int:
    return x >> 9


def shift_s(x: int) -> int:
    return (x << 9) & ON_BOARD_MASK


def shift_w(x: int) -> int:
    return (x >> 1) & (~COL8_MASK & ON_BOARD_MASK)


def shift_e(x: int) -> int:
    return (x << 1) & (~COL0_MASK & ON_BOARD_MASK)


def popcount(x: int) -> int:
    return bin(x & ON_BOARD_MASK).count("1")


# === Flood-fill (the novel kernel) =========================================


def flood_fill(seed_bit: int, propagate_mask: int, liberty_mask: int):
    """Returns (group_mask, group_size, has_liberty).

    Mirrors hw/ip/mcts_accel/flood_fill.sv exactly. seed_bit must already be
    contained in propagate_mask; the caller is responsible for that.
    """
    visited = seed_bit & ON_BOARD_MASK
    frontier = visited
    has_liberty = False
    while frontier:
        neighbors = (
            shift_n(frontier) | shift_s(frontier) |
            shift_w(frontier) | shift_e(frontier)
        ) & ON_BOARD_MASK
        if neighbors & liberty_mask & ~propagate_mask:
            has_liberty = True
        new_front = neighbors & propagate_mask & ~visited
        if not new_front:
            break
        visited |= new_front
        frontier = new_front
    return visited, popcount(visited), has_liberty


# === Bitboard ↔ 2D-array conversion (for hand-written tests) ===============


def board_from_grid(rows):
    """rows: list[str] of 9 strings of length 9 with chars '.', 'b', 'w'."""
    assert len(rows) == 9 and all(len(r) == 9 for r in rows)
    black = white = 0
    for r, row in enumerate(rows):
        for c, ch in enumerate(row):
            idx = r * 9 + c
            if ch == "b":
                black |= 1 << idx
            elif ch == "w":
                white |= 1 << idx
            elif ch != ".":
                raise ValueError(f"bad char {ch!r}")
    return black, white


def grid_from_board(black: int, white: int):
    rows = []
    for r in range(9):
        s = []
        for c in range(9):
            idx = r * 9 + c
            b = (black >> idx) & 1
            w = (white >> idx) & 1
            if b and w:
                s.append("?")
            elif b:
                s.append("b")
            elif w:
                s.append("w")
            else:
                s.append(".")
        rows.append("".join(s))
    return rows


# === 16-bit Fibonacci LFSR (matches lfsr16.sv) =============================


def lfsr16_advance(state: int) -> int:
    if state == 0:
        state = 0xACE1
    fb = ((state >> 15) ^ (state >> 14) ^ (state >> 12) ^ (state >> 3)) & 1
    return ((state << 1) & 0xFFFF) | fb


# === Move application with full rule semantics =============================


def play_move(black: int, white: int, turn: int, cell_idx: int):
    """Try to play `turn` at cell_idx. Returns (new_black, new_white,
    legal, captured_count). Mirrors rollout_engine.sv TRY_PLACE → CAPTURE_FF
    → SUICIDE_FF logic (no ko check — caller does that)."""
    bit = 1 << cell_idx
    if (black | white) & bit:
        return black, white, False, 0
    if turn == 0:  # black to move
        new_black = black | bit
        new_white = white
        own = new_black
        enemy = new_white
        enemy_is_white = True
    else:
        new_white = white | bit
        new_black = black
        own = new_white
        enemy = new_black
        enemy_is_white = False

    captured = 0
    # Check each of 4 neighbors of just-placed stone for enemy stones.
    empties = ~(new_black | new_white) & ON_BOARD_MASK
    for shift_fn in (shift_n, shift_s, shift_w, shift_e):
        nbr = shift_fn(bit) & enemy
        if not nbr:
            continue
        group, size, has_lib = flood_fill(nbr, enemy, empties & ~bit)
        if not has_lib:
            enemy &= ~group
            captured += size

    if enemy_is_white:
        new_white = enemy
    else:
        new_black = enemy

    # Suicide check.
    empties = ~(new_black | new_white) & ON_BOARD_MASK
    own = new_white if turn else new_black
    _, _, own_has_lib = flood_fill(bit, own, empties)
    if not own_has_lib:
        return black, white, False, 0

    return new_black, new_white, True, captured


def score(black: int, white: int):
    """Tromp-Taylor area scoring. Returns (b_score, w_score) without komi."""
    b_stones = popcount(black)
    w_stones = popcount(white)
    visited_empty = 0
    b_terr = w_terr = 0
    empties = ~(black | white) & ON_BOARD_MASK
    remaining = empties
    while remaining:
        seed = remaining & -remaining  # lowest set bit
        group, size, _ = flood_fill(seed, empties, black | white)
        # Check what colors border this empty region.
        bdy = (shift_n(group) | shift_s(group)
               | shift_w(group) | shift_e(group)) & ON_BOARD_MASK
        touches_b = bool(bdy & black)
        touches_w = bool(bdy & white)
        if touches_b and not touches_w:
            b_terr += size
        elif touches_w and not touches_b:
            w_terr += size
        visited_empty |= group
        remaining &= ~group
    return b_stones + b_terr, w_stones + w_terr


def winner(black: int, white: int):
    """Returns 0 if black wins, 1 if white wins (with komi tiebreak)."""
    b, w = score(black, white)
    return 0 if b > w + KOMI_INT else 1


# === Rollout (matches rollout_engine.sv inner FSM) =========================


def select_legal(black: int, white: int, turn: int, lfsr_state: int):
    """Pick a random legal move. Up to 5 suicide retries, then pass."""
    empties = ~(black | white) & ON_BOARD_MASK
    legal_count = popcount(empties)
    state = lfsr_state
    for _ in range(5):
        if legal_count == 0:
            return None, state  # pass
        state = lfsr16_advance(state)
        idx = state % legal_count
        # find idx-th set bit
        seen = 0
        cell = -1
        for i in range(81):
            if (empties >> i) & 1:
                if seen == idx:
                    cell = i
                    break
                seen += 1
        nb, nw, legal, _ = play_move(black, white, turn, cell)
        if legal:
            return cell, state
    return None, state  # pass after 5 retries


def rollout(black: int, white: int, turn_in: int,
            ko_b: int, ko_w: int, seed: int, move_cap: int = 162):
    """One rollout to terminal. Returns (first_cell, winner_color).

    first_cell: 0..80 = the cell played as the rollout's first move; 81 = pass.
    winner_color: 0 = black wins, 1 = white wins.
    """
    lfsr_state = (seed ^ 0x5A5A) & 0xFFFF or 0xACE1
    turn = turn_in
    move_count = 0
    consecutive_passes = 0
    first_cell = 81
    cur_ko_b = ko_b
    cur_ko_w = ko_w

    while consecutive_passes < 2 and move_count < move_cap:
        cell, lfsr_state = select_legal(black, white, turn, lfsr_state)
        if cell is None:
            if move_count == 0:
                first_cell = 81
            consecutive_passes += 1
            turn ^= 1
            move_count += 1
            continue
        prev_b, prev_w = black, white
        nb, nw, legal, _ = play_move(black, white, turn, cell)
        if not legal:
            consecutive_passes += 1
            turn ^= 1
            move_count += 1
            continue
        if nb == cur_ko_b and nw == cur_ko_w:
            # ko violation — pass
            consecutive_passes += 1
            turn ^= 1
            move_count += 1
            continue
        if move_count == 0:
            first_cell = cell
        cur_ko_b, cur_ko_w = prev_b, prev_w
        black, white = nb, nw
        consecutive_passes = 0
        turn ^= 1
        move_count += 1

    return first_cell, winner(black, white)


def rollout_batch(black: int, white: int, turn_in: int,
                  ko_b: int, ko_w: int, seed: int, sims: int):
    """N rollouts, accumulate (wins, visits) per first-cell. Returns
    (wins[82], visits[82]) for cells 0..80 and pass=81."""
    wins = [0] * 82
    visits = [0] * 82
    for s in range(sims):
        sub_seed = (seed * 1103515245 + s * 12345 + 2654435761) & 0xFFFFFFFF
        first_cell, winner_color = rollout(black, white, turn_in,
                                           ko_b, ko_w, sub_seed)
        visits[first_cell] += 1
        # win counts for the root mover (turn_in side)
        if winner_color == turn_in:
            wins[first_cell] += 1
    return wins, visits
