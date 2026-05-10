# Phase 3 — Cursor + USB Keyboard

**Goal:** boot the board, plug in a USB keyboard, run `go_phase3`, and use arrow keys to move a green ring around the 9×9 grid. Enter places a stone (alternating black/white). R resets. Esc quits. **No game logic yet** — Phase 4 adds rule enforcement.

**Prerequisites:** Phase 2 must have rendered stones via `go_test`. If that worked, Phase 3 is small additions to both sides — one new register and one input loop.

## What changed in this commit

- **`hw/go_peripheral.sv`** — added `CURSOR` register at offset 0x04, registered cursor state, cursor-ring rendering placed *above* stones in the priority chain so the cursor is always visible.
- **`hw/go_peripheral_hw.tcl`** — unchanged (no new files; same Avalon shape).
- **`sw/usbkeyboard.{c,h}`** — verbatim from FlappyBird's reference (with attribution comment). Standard libusb-1.0 keyboard scan-code reader.
- **`sw/go_phase3.c`** — interactive bring-up program: keyboard polling loop drives cursor and stone placement.
- **`sw/Makefile`** — added `go_phase3` target with `-lusb-1.0`.

## Register map (Phase 3)

| Offset | Name        | W bits                   | Effect                   |
|--------|-------------|--------------------------|--------------------------|
| 0x00   | SET_BLACK   | data[6:0] = cell_idx     | cells[idx] = BLACK       |
| 0x01   | SET_WHITE   | data[6:0] = cell_idx     | cells[idx] = WHITE       |
| 0x02   | CLEAR_CELL  | data[6:0] = cell_idx     | cells[idx] = EMPTY       |
| 0x03   | RESET_BOARD | any                      | all cells = EMPTY        |
| 0x04   | CURSOR      | {visible[7], cell_idx[6:0]} | green ring at cell, on/off |
| 0x05..0x07 | reserved | —                        | Phase 5 / 8              |

**Cursor reset value:** at FPGA reset, `cursor_visible = 1`, `cursor_idx = 40` (tengen, row 4 col 4). Software can override at any time by writing 0x04.

## Step 1 — Re-import the Qsys IP? **No, not needed.**

The `_hw.tcl` file did not change. The Avalon slave shape is the same. Just rebuild:

```bash
cd hw/
make qsys      # picks up the new go_peripheral.sv source
make quartus
make rbf
```

If Quartus complains it can't find `board_mem.sv` or similar, then the IP cache is stale — open Platform Designer, right-click `go_peripheral_0` → "Refresh Component" → save → re-Generate HDL.

## Step 2 — Build the userspace program (on the DE1-SoC)

Phase 3 needs `libusb-1.0`. Confirm it's installed:

```bash
dpkg -l | grep libusb-1.0    # Debian-based images (likely)
# or
pkg-config --exists libusb-1.0 && echo OK
```

If not present: `sudo apt-get install libusb-1.0-0-dev`.

Then:

```bash
cd /path/to/sw
make go_phase3
```

If linking fails with `-lusb-1.0` not found, install `libusb-1.0-0-dev` (the `-dev` package provides the header + `.so` symlink).

## Step 3 — Run

Plug a USB keyboard into either USB Type-A port on the DE1-SoC. Then:

```bash
sudo ./go_phase3
```

Expected output on the terminal:
```
Phase 3 interactive: arrows = move cursor, Enter = place,
                     R = reset, Esc = quit.
```

On the VGA monitor:
- Empty board, hoshi dots visible
- A **green ring** at the tengen (4,4) intersection
- Press arrows: ring moves one cell at a time, clamped to [0,8] × [0,8]
- Press Enter: a black stone appears under the ring
- Press Enter again: a white stone appears at the new cursor position
- Press R: board clears, cursor stays put, next stone is black again
- Press Esc: ring disappears (cursor hidden), program exits

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| `No USB keyboard found.` | Keyboard not plugged in, or it's an unusual gaming keyboard that doesn't speak the boot protocol. Try a basic wired keyboard. |
| `mmap: Permission denied` | Not running as root. |
| Ring visible but doesn't move | Keyboard reports are arriving but key transitions aren't being detected. Add a `printf("key=0x%02x\n", key)` inside the loop to debug. |
| Ring moves but stones don't place on Enter | Check `KEY_ENTER = 0x28` matches your keyboard. Some keyboards report Return as 0x28, NumPad-Enter as 0x58. |
| Stones place at wrong cells | Bit ordering in `cursor_set()` vs. `place_stone()` — both use `cell_idx = row*9 + col`. Verify both. |
| Cursor ring partly missing | The `d2 ∈ [264, 400]` band might be too thin near corners; increase to `[250, 410]` for a wider ring. |
| Cursor disappears under stones | The priority order in `go_peripheral.sv always_comb` is wrong — `on_cursor` should be checked BEFORE `in_stone`. |

## What we are NOT doing in Phase 3

- No game rules — pressing Enter on an occupied cell silently overwrites the stone (Phase 4 will reject illegal moves).
- No score / capture detection (Phase 4 + Phase 5).
- No menu / mode select (Phase 8).
- Holding an arrow key does NOT auto-repeat — single press = single move. Add a poll counter if we want autorepeat in Phase 4+.

## Phase 3 → Phase 4 transition notes

Phase 4 ports `references/Chess/source/sw/chess.c`-style game logic into `sw/board.c`:
- `board_place(BoardState *b, int row, int col)` — full move validation
- Liberty counting via flood-fill
- Zobrist ko detection
- Capture removal

Phase 4 changes nothing in hardware. The interactive loop will move from `go_phase3.c` into a higher-level `go_main.c` that:
1. Reads keyboard input
2. Calls `board_place()` (which handles legality)
3. On `MOVE_OK`, walks the diff and writes individual `SET_*` / `CLEAR_CELL` registers for each changed cell.
