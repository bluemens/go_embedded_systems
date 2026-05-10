# Phase 5 — Score Strip Framebuffer

**Goal:** the bottom 60 pixels of the screen show a score panel with live captured-stone counts and turn indicator. On game over, that area transforms into a "GAME OVER" banner with the winner. All software-rendered with a 5×7 bitmap font into a BRAM-backed double-buffered framebuffer.

**Prerequisites:** Phases 1–4 working — empty-board render, tilemap stones, cursor + keyboard, rule-enforced PvP. Phase 5 adds a second Avalon slave on the same Qsys component, so this is the heaviest hardware delta yet.

## What changed in this commit

- **`hw/strip_fb.sv`** — new module. 640×60 8bpp double-buffered BRAM:
  - Two 9,600 × 32-bit banks (≈30 M10K blocks each).
  - Vsync-aligned swap when `swap_request` is armed and `vsync_pulse` fires.
  - 1-cycle BRAM read latency on the VGA side; only manifests as a possible 1-pixel horizontal offset within the strip, invisible in practice.
- **`hw/go_peripheral.sv`** — extended:
  - New `avalon_slave_1` ports for the 32-bit pixel write window.
  - `STRIP_SWAP` register at offset 0x05 of `avalon_slave_0`.
  - VS rising-edge detector emits a 1-cycle `vsync_pulse`.
  - Strip pixel address = `(py − 420) × 640 + px`, fed to `strip_fb`.
  - Color decision: when `py ∈ [420, 479]`, use the strip pixel through a small inline 8-color palette (case statement on `strip_pixel[2:0]`). Outside the strip, the existing board renderer applies.
- **`hw/go_peripheral_hw.tcl`** — added `avalon_slave_1` interface declaration with port names `strip_writedata` / `strip_write` / `strip_chipselect` / `strip_address`. Added `strip_fb.sv` to the synthesis fileset.
- **`sw/font5x7.{h,c}`** — minimal 5×7 ASCII font: digits, A–Z, ' ', '.', ':', '=', '-', '/', '!'. Lowercase upcased. Unsupported chars render as a placeholder rectangle.
- **`sw/strip_render.{h,c}`** — local 38,400-byte rendering buffer + primitives (`strip_clear`, `strip_pixel`, `strip_rect`, `strip_char`, `strip_text`) + `strip_present()` which bulk-writes the buffer to FPGA and arms a swap.
- **`sw/go_main.c`** — `render_panel()` draws score / turn / capture counts (or game-over banner) and calls `strip_present()` after every state change.
- **`sw/Makefile`** — `go_main` target now links `strip_render.c` and `font5x7.c`.

## Register map (Phase 5)

### avalon_slave_0 (control registers, 8 bytes)
| Off | Name        | Bits                       | Effect |
|-----|-------------|----------------------------|--------|
| 0x00 | SET_BLACK   | data[6:0] = cell_idx       | place black |
| 0x01 | SET_WHITE   | data[6:0] = cell_idx       | place white |
| 0x02 | CLEAR_CELL  | data[6:0] = cell_idx       | clear |
| 0x03 | RESET_BOARD | any                        | clear all |
| 0x04 | CURSOR      | {visible[7], cell_idx[6:0]} | cursor |
| 0x05 | STRIP_SWAP  | any                        | arm swap at next VS |
| 0x06..0x07 | reserved | — | Phase 8 |

### avalon_slave_1 (strip framebuffer, 38,400 bytes / 9,600 words / 64 KB span)
- 32-bit data, 14-bit word address.
- `word w` byte layout: pixels at strip indices `4w`, `4w+1`, `4w+2`, `4w+3` in bits `[7:0]`, `[15:8]`, `[23:16]`, `[31:24]`.
- Writes go to the BACK buffer; swap brings the new contents on screen.

### Pixel encoding (8 bits)
Bottom 3 bits index a tiny inline palette; top 5 bits ignored.

| idx | RGB888    | Use                       |
|-----|-----------|---------------------------|
| 0   | 0x202020  | bg dark gray              |
| 1   | 0xF0F0F0  | white text                |
| 2   | 0x808080  | dim gray                  |
| 3   | 0x00C040  | green accent              |
| 4   | 0xFFD700  | gold (winner)             |
| 5   | 0xC04040  | red (error)               |
| 6   | 0x4080C0  | blue accent               |
| 7   | 0xDEB887  | burlywood (board match)   |

## Step 1 — Re-import the Qsys IP (because `_hw.tcl` got a new slave)

In Platform Designer:
1. Remove existing `go_peripheral_0`.
2. From IP Catalog, add it back. The Component Editor preview should now show **two** Avalon slave interfaces (`avalon_slave_0` and `avalon_slave_1`) plus the existing clock / reset / vga conduit.
3. Re-wire:
   - `clock` → `clk_0.clk`
   - `reset` → `clk_0.clk_reset`
   - `avalon_slave_0` → `hps_0.h2f_lw_axi_master`
   - **`avalon_slave_1` → `hps_0.h2f_lw_axi_master`** (same master, different base offset)
   - `vga` → exported
4. **System → Address Map** (or right-click each slave's Base column):
   - Set `avalon_slave_0` base to `0x0000`
   - Set `avalon_slave_1` base to `0x10000`
   - This matches `STRIP_FB_OFFSET = 0x10000` in `go_main.c`. If you assign different offsets, edit the constant.
5. Save. Generate HDL.

## Step 2 — Patch toplevel? **No.** The conduit ports are unchanged.

The `vga` conduit names are identical to Phase 1; `soc_system_top.sv` already wires them correctly.

## Step 3 — Compile

```bash
cd hw/
make qsys      # ~30s
make quartus   # ~5 min — adds two BRAMs (~60 M10K blocks)
make rbf
```

If timing fails (Quartus says "Slack: -X.Y ns at 50 MHz"): the strip pixel-address multiply is the most likely offender. Mitigation: register `(py - 420)` and `px` separately, do the multiply in a clocked stage. We'll come back to this if it actually fails — Quartus often pipelines it for us.

## Step 4 — Build software (on the DE1-SoC)

```bash
cd /path/to/sw
make go_main
sudo ./go_main
```

## Expected behavior

| State | What's on screen |
|-------|------------------|
| Boot | Empty board, green cursor at tengen, score panel reads "BLACK: 0  WHITE: 0  TURN: BLACK". |
| After Black plays | Same panel, "TURN: WHITE" updates. |
| After a capture | "CAP B=N W=M" updates with the count of captured-by-each-side. |
| One pass | Bottom-right of strip shows "1 PASS" in green. |
| Two passes | Strip transforms: "GAME OVER" gold, score line + winner. |
| R | Strip resets to live score panel, board clears. |

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Strip area is solid color (no text) | `strip_present()` not called; or `STRIP_SWAP` writing wrong offset; or Qsys slave_1 base not at 0x10000. |
| Strip shows garbage / random pixels | The local `scratch[]` buffer is fine but the bulk-write loop is hitting wrong addresses. Check `STRIP_FB_OFFSET = 0x10000` matches Qsys. |
| Strip flickers between two contents | Swap firing more than once per `strip_present()`. The `swap_armed` register should latch and clear on VS pulse. |
| Strip is offset 1 pixel horizontally | 1-cycle BRAM read latency. Cosmetic; we'll fix if it's visible by registering the board pipeline to match. |
| Text appears mirrored or upside down | Font byte order: bit 0 should be the TOP row; check `font5x7.c` glyph definitions match this convention. |
| `cc: error: undefined reference to libusb...` | Missing `-lusb-1.0` — should be in the Makefile go_main target. |

## What's NOT in Phase 5

- No menus (Phase 8). Title screen, mode select, difficulty select still happen via the terminal.
- No audio (Phase 6).
- Strip is fixed at the bottom 60 px; if we want a full-screen menu, we'd need either a different render-mode register or to expand the strip region — Phase 8 will decide.
- The font is uppercase-only and minimal punctuation. Adding lowercase or more symbols is just more glyph bytes in `font5x7.c`.

## Phase 5 → Phase 6 transition notes

Phase 6 adds audio:
- New `audio_controller.sv` — Avalon-ST source streaming 16-bit samples at 8 kHz to a Qsys University Program audio core.
- ROM banks loaded via `$readmemh` from `.vh` files (place / capture / illegal / game_over).
- `AUDIO_CMD` register at offset 0x06.
- New audio core component in Qsys, plus its PLL.

This is a new HDL file plus more Qsys integration but no further changes to the Avalon slave shape we already have.
