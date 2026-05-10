# Phase 4 — Rule-Enforced PvP

**Goal:** complete a 9×9 Go game between two humans on the DE1-SoC with full rule enforcement (occupancy / suicide / ko / capture / pass / two-pass game-over / Chinese-style scoring). No AI yet (Phase 7).

**Prerequisites:** Phase 3 must work — cursor moves, Enter places stones (with no rule check). Phase 4 adds the rule engine in software; **no FPGA changes**.

## What changed in this commit

- **`sw/board.h`** + **`sw/board.c`** — the game rule engine (~330 lines C, no hardware deps).
  - `BoardState` struct (cells, turn, captures, ko hash, pass counter, game_over flag).
  - `board_init()`, `board_place()`, `board_pass()`, `board_score()`.
  - Liberty counting via BFS flood-fill (4-connected).
  - Capture detection: any opponent group adjacent to a placed stone with 0 liberties is removed.
  - Ko detection: 64-bit Zobrist hash compared against the position before the opponent's last move (catches simple ko, not positional super-ko — sufficient for our scope).
- **`sw/board_test.c`** — host-runnable unit tests. No FPGA needed; builds + runs on Mac/Linux.
- **`sw/go_main.c`** — replaces `go_phase3.c` as the main game program. Calls `board_place()` instead of writing cells directly. After each successful move, pushes the entire 81-cell tilemap to the FPGA (~6 µs total).
- **`sw/Makefile`** — added `go_main` and `board_test` targets.

**No HDL changes**, so:
- No `make qsys`, no Quartus re-run needed for Phase 4.
- The FPGA bitstream from Phase 3 still works.

## Step 1 — Verify rule engine on your laptop

Before deploying anything, run the unit tests locally:

```bash
cd sw/
cc -O2 -Wall board_test.c board.c -o board_test
./board_test
```

Expected output:
```
test_init
test_basic_placement
test_simple_capture
test_suicide
test_ko
test_two_passes_end_game
test_simple_score

All tests passed.
```

If any FAIL, **don't deploy** until you fix the rule engine. Any rule bug will play out in real games and be much harder to diagnose with a VGA monitor + USB keyboard than with `printf`.

## Step 2 — Build on the DE1-SoC

```bash
cd /path/to/sw
make go_main
```

If `libusb-1.0` is already installed (from Phase 3), this just compiles. If not:
```bash
sudo apt-get install libusb-1.0-0-dev
```

## Step 3 — Run

```bash
sudo ./go_main
```

Terminal output:
```
Phase 4 PvP. Black to play.
Controls: arrows = cursor, Enter = place,
          Space = pass, R = restart, Esc = quit.
```

Expected gameplay (some examples to try):

| Action | Expected |
|--------|----------|
| Move cursor with arrows | Green ring follows |
| Press Enter on empty intersection | Stone appears, "Black plays (r,c)..." printed, turn passes to White |
| Press Enter on occupied cell | "Illegal: occupied" printed; nothing happens |
| Set up a 4-stone surround of an opponent stone, then place the closing move | Opponent stone disappears, "Captured: B=N W=M" updates |
| Set up a ko shape and try to immediately recapture | "Illegal: ko" printed |
| Try suicide (place into a fully-surrounded empty point with no captures) | "Illegal: suicide" printed |
| Press Space twice consecutively | "GAME OVER" with Chinese-area score, +5.5 komi for White, winner declared |
| Press R | "--- new game ---", board clears, Black to play again |
| Press Esc | Exit |

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Stone placed but no print on terminal | stdout buffering — pipe through `stdbuf -oL ./go_main` or add fflush |
| Capture happens but stones don't disappear from screen | `hw_push_board()` is iterating wrong; or board.c isn't actually clearing cells. Add a printf to confirm board.c is doing it. |
| Ko reported on a non-ko position | Zobrist seeding diverged between this run and the previous prev_board_hash. Should not happen since seed is fixed; but if you see it, check `srand(0xC0FFEE)` is hit on `board_init`. |
| Suicide rejected when it shouldn't be (i.e., the move actually captures) | `process_captures` runs before `liberties_and_group(self)` — the order in `board_place` matters. |
| "Game over" never reached | `board_pass` increments `consecutive_passes` per pass; verify it's not being reset by any path other than `board_place` |

## What's NOT in Phase 4

- No score panel on the VGA monitor (it's only printed to the terminal). Phase 5 adds the strip framebuffer for on-screen score.
- No audio feedback for placements / illegal moves / captures (Phase 6).
- No AI (Phase 7).
- No menu system / mode select / game-over screen on VGA (Phase 8).
- No automatic dead-stone detection at game end. The Chinese-area scoring assumes both players have removed all dead stones (i.e., played out the game cleanly). If players pass while dead stones remain, the score will favor the player whose dead stones are still on the board. This is standard for engine-implemented Go and matches what real Chinese-rules games do.
- No positional super-ko (only simple ko). For 9×9 amateur play this is fine; it will only matter for triple-ko or sending-two-returning-one situations that are vanishingly rare.

## Phase 4 → Phase 5 transition notes

Phase 5 adds the 640×60 strip framebuffer at the bottom of the screen for on-screen text:
- Score: "BLACK: NN  WHITE: NN  CAP: B=N W=N"
- Whose turn (already on the cursor color when we add palette in Phase... wait, we didn't add per-turn cursor color)
- Game-over banner

Phase 5 needs a **second Avalon slave** on the go_peripheral component (38,400-byte write window for the strip back buffer + a STRIP_SWAP register). This means another Qsys integration cycle: edit `_hw.tcl` to add the second slave interface, re-import in Platform Designer, regenerate.

OR we can keep one slave and widen the address bus. Both viable; we'll choose based on timing constraints when we get there.
