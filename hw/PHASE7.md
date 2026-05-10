# Phase 7a — Computer Player (Random / Greedy / Serial MCTS)

**Goal:** play against the computer at three difficulty levels. Run `./go_main 1`, `2`, or `3` and the AI takes White; you play Black with arrow keys + Enter as before. Phase 7a is **CPU-only**; the FPGA rollout accelerator (Phase 7b) is deferred until we know whether MCTS at 200 sims is fast enough on the Cortex-A9.

**Prerequisites:** Phases 1–6 working. Phase 7a is **purely software** — no HDL changes, no Qsys re-import. The FPGA bitstream from Phase 6 still works.

## What changed in this commit

- **`sw/ai.h`** + **`sw/ai.c`** (~330 lines):
  - **Level 1, random** — uniform-random over legal moves; pass if none.
  - **Level 2, greedy** — heuristic scoring: `+10` per captured stone, `+8` for saving an own group from atari, `+2` per opponent liberty reduced, small center bias. Random tiebreak.
  - **Level 3, MCTS** — UCT with `√2` exploration. 200 simulations per move, max rollout depth 324 moves. Fixed-size 6,000-node pool (no malloc per node). Rollouts use a fast-play path: no ko check, just suicide rejection.
  - `ai_seed(seed)` to make games non-deterministic; called from `time(NULL)`.
- **`sw/go_main.c`**:
  - Argv parses `./go_main [N]` where N=1/2/3 selects PvC mode at that level. No arg = PvP.
  - When AI's turn (always WHITE in PvC), runs `ai_get_move()` and applies via `board_place` → `hw_push_board` → `render_panel` → `hw_play_audio(PLACE/CAPTURE)`.
  - In PvC, Enter on a Black-to-move board does the human move *then* triggers the AI's reply (so each player keypress advances both turns).
  - "AI thinking..." stdout print; flush before the AI call so it's visible.
- **`sw/Makefile`** — adds `ai.c` to `go_main` deps; links `-lm` for `log()`/`sqrt()` in MCTS UCB1.

## Build & run

On the DE1-SoC:

```bash
cd /path/to/sw
make go_main         # picks up ai.c automatically

sudo ./go_main       # PvP (existing behavior)
sudo ./go_main 1     # PvC, AI = random (instant)
sudo ./go_main 2     # PvC, AI = greedy (instant; surprisingly competent)
sudo ./go_main 3     # PvC, AI = MCTS 200 sims (~1-3 sec per move)
```

## Expected behavior

- **Level 1 (Random)** — opponent plays terribly, useful to validate the path. Each move is sub-millisecond.
- **Level 2 (Greedy)** — surprisingly hard for a casual player; will capture aggressively and defend ataris. Each move is sub-5-millisecond.
- **Level 3 (MCTS)** — strongest. With 200 sims per move you'll see the AI think for 1–3 seconds then place a strategically reasonable stone. No tactical reading; pure rollout-based.

## Performance budget verification (the open question from design doc §13)

The §13 risk was: "200 simulations × 81-move playouts = 16,200 random move generations per AI turn. Need to validate < 2 s on real hardware."

What our code does per simulation:
1. Selection: descend ~10 levels via UCB1 → 10 × `legal_moves()` (each is 81× `can_play_fast`).
2. Expansion: 1 node alloc, 1 `legal_moves()`.
3. Rollout: up to 324 moves, each with `legal_moves()` + 1 random pick.
4. Backprop: walk parent chain, increment counters.

Worst-case rollout cost: 324 × 81 = ~26,000 `can_play_fast` calls. Each `can_play_fast` does a board copy (162 bytes) + a flood-fill (≤81 cells). Estimated ~3 µs per call on Cortex-A9 = 80 ms per rollout.

200 rollouts × 80 ms = 16 s. **That's worse than the budget.**

If real-world measurements confirm this, mitigations in order:
1. **Drop sims to 100** — cuts time in half, modest strength loss.
2. **Reduce rollout max depth to 162 (2×N²)** — usually rollouts terminate in ~50 moves anyway via passes.
3. **Cache pseudo-legal moves between calls** — same board state in selection vs. rollout init.
4. **Phase 7b: FPGA rollout accelerator** — see below.

## Validation plan (run after Phase 7a is built)

```bash
# On the DE1-SoC, time a single MCTS turn:
sudo ./go_main 3
# (play Black at tengen, then time the AI response)
```

Or instrument `apply_ai_move()` with a `clock_gettime(CLOCK_MONOTONIC)` pair and print elapsed ms.

If the budget holds (<2 s/turn), we can stop here. If not, escalate to Phase 7b.

## Phase 7b — FPGA Rollout Accelerator (deferred)

Per design doc §7.5 and the new `[MPC]` / `[Systolic]` references, the rollout phase is the candidate for hardware offload:

- **N parallel uniform-random playout engines** in fabric, each with its own bitboard state and PRNG.
- HPS sends a root position via Avalon writes (162 bits of board state + turn + ko hash, fits in a few words).
- HPS triggers a "run M rollouts" command; FPGA returns aggregate (wins, total) for the position.
- Each engine: bitboard-based legality check (much cheaper than the C BFS), 81-bit tight loop for rollouts, terminal-position scoring.

Architecture per [MPC] / [Systolic] patterns:
- Top FSM (idle → loading → running → reporting) — like `[MPC]/admm_solver.sv`.
- BRAM-mapped Avalon slave to receive root state and return results — like `[MPC]/memory_interface.sv`.
- `genvar` fan-out of N engines — like `[Systolic]/PE_array.v`.
- HPS polls a "done" register, then reads results.

This is a substantial new module (~600 lines HDL + matching software). Recommend tackling it **only after measuring real-world Phase 7a latency on hardware**.

## What's NOT in Phase 7a

- No FPGA accelerator (Phase 7b).
- No opening book / pattern matching for the greedy heuristic.
- No symmetry exploitation (could play random tengen response if cooperatively chosen).
- No time control / move-time limit.
- The MCTS rollout's "random legal move generation" rebuilds the legal moves list from scratch each time. A more efficient implementation would cache the list and incrementally update on stone placements/captures.
- No tournament-mode game-state save/load.

## Phase 7 → Phase 8 transition

Phase 8 polishes the user experience:
- **Title screen** ("GO 9×9", press Enter to start).
- **Mode select menu** — PvP / PvC arrow-key navigation.
- **Difficulty select** — Level 1 / 2 / 3 in PvC mode.
- **Game-over screen** with bigger banner (already partially done).

These all draw to the strip framebuffer using `strip_render`, gated by a `RENDER_MODE` register that tells `go_peripheral.sv` whether to draw the board area or a "hidden behind a banner" mode for menus. Phase 8 adds maybe 200 lines of UI code and 1 register; no new HDL modules.
