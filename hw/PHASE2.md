# Phase 2 — Tilemap + HPS-controlled Stones

**Goal:** the HPS can place / clear / reset stones on the 9×9 board by writing single-byte Avalon registers. The FPGA renders stone circles procedurally based on the tilemap. PvP-by-keyboard is not yet possible — that's Phase 3 (cursor + libusb).

**Prerequisite:** Phase 1 must have rendered a static board on VGA. If it did, Phase 2 is a much smaller delta — the Avalon slave shape is unchanged, so Qsys integration is *almost* unchanged.

## What changed in this commit

- **`hw/go_peripheral.sv`** — added register decoder, `board_mem` instantiation, and stone-circle rendering. `vga_counters` and the geometry are unchanged.
- **`hw/board_mem.sv`** — new file: 81-cell × 2-bit tilemap with synchronous write port and combinational read.
- **`hw/go_peripheral_hw.tcl`** — added `board_mem.sv` to the synthesis fileset. **This change requires re-importing the IP in Platform Designer** (see step 1 below).
- **`sw/go_test.c`** — new userspace test program using the `/dev/mem` mmap pattern.
- **`sw/Makefile`** — added `go_test` build target.

## Register map (Phase 2)

Byte-addressed within the go_peripheral region (which sits at offset 0x0000 inside the LW HPS-to-FPGA bridge at physical 0xFF200000):

| Offset | Name        | W bits      | Effect                                 |
|--------|-------------|-------------|----------------------------------------|
| 0x00   | SET_BLACK   | data[6:0] = cell_idx | cells[cell_idx] = BLACK       |
| 0x01   | SET_WHITE   | data[6:0] = cell_idx | cells[cell_idx] = WHITE       |
| 0x02   | CLEAR_CELL  | data[6:0] = cell_idx | cells[cell_idx] = EMPTY       |
| 0x03   | RESET_BOARD | data[0] = pulse      | all 81 cells = EMPTY in 1 cycle |
| 0x04..0x07 | reserved | —                    | Phase 3: cursor / render mode  |

`cell_idx = row * 9 + col` for `row, col ∈ [0, 8]`. Range [0, 80]; values ≥ 81 are silently ignored by `board_mem`.

## Step 1 — Re-import the Qsys IP (because `_hw.tcl` changed)

On a class machine with Quartus 21.1:

1. Open Platform Designer (`Tools → Platform Designer`, then `File → Open soc_system.qsys`).
2. In **System Contents**, right-click `go_peripheral_0` and choose **Remove**. (We're going to re-add it with the updated fileset.)
3. In the **IP Catalog**, locate "Go Peripheral" → double-click → instance name `go_peripheral_0` → **Finish**.
4. Re-wire the four connections (same as Phase 1 step 2.3):
   - `clock` → `clk_0.clk`
   - `reset` → `clk_0.clk_reset`
   - `avalon_slave_0` → `hps_0.h2f_lw_axi_master`
   - `vga` → exported (double-click the **Export** column)
5. **System → Assign Base Addresses**. Confirm `go_peripheral_0.avalon_slave_0` is at `0x00000000` again. (If not, edit `GO_PERIPHERAL_OFFSET` in `sw/go_test.c` to match.)
6. **Save** (`Ctrl+S`).
7. **Generate HDL** (top-right). Choose VERILOG. Confirm no errors.

`soc_system_top.sv` does **not** need any changes — the Qsys-exported port names are identical to Phase 1.

## Step 2 — Build & program

```bash
cd hw/
make qsys      # ~30s
make quartus   # ~3-5 min
make rbf       # ~5s
```

Copy `output_files/soc_system.rbf` to the SD card's FAT partition, reboot the DE1-SoC.

## Step 3 — Build & run the test program (on the DE1-SoC)

After Linux boots and you have a shell (UART or SSH):

```bash
cd /path/to/sw      # wherever you placed the sw/ dir on the target
make go_test        # compiles go_test.c with the on-board gcc
sudo ./go_test
```

(The `sudo` is needed because `/dev/mem` access requires root. If you're running as root by default — common on the DE1-SoC's stock image — drop the `sudo`.)

## Expected behavior

The test program runs through three stages, with the VGA monitor showing:

| Stage | What you should see |
|-------|---------------------|
| 1. After `reset_board()` | Empty board: burlywood field with black grid + 5 hoshi dots. **(2 sec pause)** |
| 2. After 5 `place_*` calls | 5 stones at the hoshi points: B/W/B/W/B reading top-left → bottom-right. Black stones are solid black; white stones are white with a thin black outline ring. **(3 sec pause)** |
| 3. `clear_cell` × 5, 1 sec apart | Stones disappear one at a time, in the order: (2,2), (2,6), (4,4), (6,2), (6,6). |
| End | Empty board again, hoshi dots restored where stones were. |

If stages 1 and 3 work but stage 2 shows stones in wrong cells: bit ordering in the cell_idx encoding is off — check that `wr8(REG_SET_BLACK, row*9 + col)` matches `bm_write_addr = writedata[6:0]` in `go_peripheral.sv`.

If stones appear at the right cells but in wrong colors: `REG_SET_BLACK = 3'h0` and `bm_write_data = 2'b01` for black are the canonical values; verify both ends.

If the white stone has no outline / is invisible against the burlywood: the d² ≥ 289 outline test in `go_peripheral.sv` is the issue. Adjust the threshold or change `COLOR_WHITE`.

## What we are NOT doing in Phase 2

- No keyboard input (Phase 3)
- No game logic / move validation (Phase 4) — the test program writes raw cell-state, no rule enforcement
- No score panel / strip framebuffer (Phase 5)
- No audio (Phase 6)
- No menus or game-over screen (Phase 8)
- The Avalon slave is still 8-bit / 3-bit — we will widen in Phase 5 when we add the strip framebuffer, OR we'll add a second Avalon slave for the strip and keep this narrow one

## Phase 2 → Phase 3 transition notes

Phase 3 will add:
- `CURSOR` register at offset 0x04 (visible/row/col packed in 8 bits)
- A green ring drawn at the cursor position by `go_peripheral.sv`
- `sw/usb_kbd.c` for libusb-based keyboard polling (`references/_starter/lab2/` is the reference)

The test loop in `go_test.c` will be replaced (or supplemented) by an interactive program that watches the keyboard and drives the cell registers directly. No game logic yet — just cursor + place stone wherever you click Enter.
