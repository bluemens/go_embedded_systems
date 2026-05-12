"""Generate test vectors for the SystemVerilog testbenches.

Outputs one file per testbench under sim/vectors/, each line containing:
    inputs... outputs...

Format: hex literals, no '0x' prefix, fields separated by single spaces. The
SystemVerilog testbenches use $fscanf with "%h" to consume them.
"""

from __future__ import annotations

import argparse
import os

import go_model as gm

VEC_DIR = os.path.join(os.path.dirname(__file__), "..", "..", "sim", "vectors")


def hex81(x: int) -> str:
    return f"{x & gm.ON_BOARD_MASK:021x}"


def hex16(x: int) -> str:
    return f"{x & 0xFFFF:04x}"


def hex32(x: int) -> str:
    return f"{x & 0xFFFFFFFF:08x}"


def write_flood_fill_vectors():
    """ff_*.txt:  seed_bit propagate_mask liberty_mask | group size has_lib"""
    cases = []

    # Single isolated stone with all empties → captured if no liberty
    full_b, full_w = gm.board_from_grid([
        "b........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    ])
    empties = ~(full_b | full_w) & gm.ON_BOARD_MASK
    seed = 1 << 0
    cases.append(("single_stone_with_libs", seed, full_b, empties))

    # Stone surrounded on all 4 sides (top-left corner, 2 sides off-board count as wall)
    bb, ww = gm.board_from_grid([
        "wb.......",
        "bw.......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    ])
    # The white at (0,0) has neighbors: (0,1)=b, (1,0)=b, off-board × 2.
    # Should be captured (has_liberty=False).
    empties = ~(bb | ww) & gm.ON_BOARD_MASK
    seed = 1 << 0  # the white stone at (0,0)
    cases.append(("corner_white_in_atari_taken", seed, ww, empties))

    # Snake group along top edge (row 0, cols 0-8 all black) — center row otherwise empty
    bb, ww = gm.board_from_grid([
        "bbbbbbbbb",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    ])
    empties = ~(bb | ww) & gm.ON_BOARD_MASK
    seed = 1 << 0  # any black stone
    cases.append(("snake_row0", seed, bb, empties))

    # Snake group along left edge (col 0, all rows)
    bb, ww = gm.board_from_grid([
        "b........",
        "b........",
        "b........",
        "b........",
        "b........",
        "b........",
        "b........",
        "b........",
        "b........",
    ])
    empties = ~(bb | ww) & gm.ON_BOARD_MASK
    seed = 1 << 0
    cases.append(("snake_col0", seed, bb, empties))

    # All-black 81-stone group (no liberties — but tests max-size fill)
    bb = gm.ON_BOARD_MASK
    ww = 0
    cases.append(("full_black", 1 << 40, bb, 0))

    # Eye-shape group (3x3 surrounded by opposite color)
    bb, ww = gm.board_from_grid([
        ".........",
        ".........",
        ".........",
        "...www...",
        "...w.w...",
        "...www...",
        ".........",
        ".........",
        ".........",
    ])
    empties = ~(bb | ww) & gm.ON_BOARD_MASK
    seed = 1 << (3 * 9 + 3)  # top-left of the eye-ring
    cases.append(("eye_ring", seed, ww, empties))

    # Single corner empty surrounded by black (eye)
    bb, ww = gm.board_from_grid([
        ".bb......",
        "bb.......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    ])
    empties = ~(bb | ww) & gm.ON_BOARD_MASK
    seed = empties & -empties  # first set bit
    cases.append(("corner_eye_empty", seed, empties, bb | ww))

    # Write
    os.makedirs(VEC_DIR, exist_ok=True)
    path = os.path.join(VEC_DIR, "ff_vectors.txt")
    with open(path, "w") as f:
        f.write("# flood_fill vectors\n")
        f.write("# seed propagate_mask liberty_mask group size has_lib name\n")
        for name, seed, prop, lib in cases:
            group, size, has_lib = gm.flood_fill(seed, prop, lib)
            f.write(
                f"{hex81(seed)} {hex81(prop)} {hex81(lib)} "
                f"{hex81(group)} {size:02x} {int(has_lib):x} # {name}\n"
            )
    return path


def write_rollout_vectors():
    """re_*.txt: black white turn ko_b ko_w seed | first_cell winner"""
    os.makedirs(VEC_DIR, exist_ok=True)
    path = os.path.join(VEC_DIR, "re_vectors.txt")
    cases = []

    # Empty board, black to move, several seeds
    bb = ww = 0
    for s in (0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xACEDC0DE):
        cases.append(("empty_b_to_move", bb, ww, 0, 0, 0, s))

    # Easy capture position: black has an atari to take
    bb, ww = gm.board_from_grid([
        ".bw......",
        "bw.......",
        ".w.......",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
        ".........",
    ])
    # Black playing at (0,0) captures the white stones if I set up correctly.
    cases.append(("black_capture_avail", bb, ww, 0, 0, 0, 0xFEEDFACE))

    with open(path, "w") as f:
        f.write("# rollout vectors\n")
        f.write("# black white turn ko_b ko_w seed first_cell winner name\n")
        for name, bb, ww, turn, ko_b, ko_w, seed in cases:
            first_cell, win = gm.rollout(bb, ww, turn, ko_b, ko_w, seed)
            f.write(
                f"{hex81(bb)} {hex81(ww)} {turn:x} {hex81(ko_b)} {hex81(ko_w)} "
                f"{hex32(seed)} {first_cell:02x} {win:x} # {name}\n"
            )
    return path


def write_mcts_vectors():
    """mc_*.txt: full root + seed → expected wins[82], visits[82]"""
    os.makedirs(VEC_DIR, exist_ok=True)
    path = os.path.join(VEC_DIR, "mc_vectors.txt")
    cases = [
        ("empty_b", 0, 0, 0, 0, 0, 0xDEADBEEF, 25),
        ("empty_w", 0, 0, 1, 0, 0, 0xCAFEBABE, 25),
    ]
    with open(path, "w") as f:
        f.write("# mcts_accel vectors\n")
        f.write("# black white turn ko_b ko_w seed sims wins[82] visits[82] name\n")
        for name, bb, ww, turn, ko_b, ko_w, seed, sims in cases:
            wins, visits = gm.rollout_batch(bb, ww, turn, ko_b, ko_w, seed, sims)
            wins_str = "".join(hex16(w) for w in wins)
            visits_str = "".join(hex16(v) for v in visits)
            f.write(
                f"{hex81(bb)} {hex81(ww)} {turn:x} {hex81(ko_b)} {hex81(ko_w)} "
                f"{hex32(seed)} {sims:04x} {wins_str} {visits_str} # {name}\n"
            )
    return path


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--what", choices=["ff", "re", "mc", "all"], default="all")
    args = p.parse_args()
    if args.what in ("ff", "all"):
        print("wrote", write_flood_fill_vectors())
    if args.what in ("re", "all"):
        print("wrote", write_rollout_vectors())
    if args.what in ("mc", "all"):
        print("wrote", write_mcts_vectors())
