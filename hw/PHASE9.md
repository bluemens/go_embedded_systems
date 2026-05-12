# Phase 9 — Live Demo Features

**Goal:** make the AI's compute *visible* during a demo. Two additions on top of Phase 8's UI:

1. **Heat-map overlay.** Each empty intersection on the board glows on a red→green spectrum keyed to the current MCTS win-rate of playing there as a first move. As tree-MCTS simulations accumulate, the heat-map flows live. Demo viewers can literally *see* the AI thinking.
2. **HW/SW backend toggle + live timer.** A `Tab` key flips the AI between the SW tree-MCTS (slow, strong, supplies live heat) and the HW flat-MCTS path (fast, weaker). The score strip prints the last AI move's wall-clock time, taken from a free-running 50 MHz counter inside the FPGA. A `Y` key replays the previous AI move on the *other* backend, on the same board, for the live speedup side-by-side.

**Prerequisites:** Phases 1–7b working. Phase 9 adds one new HDL file (`heat_map.sv`), widens the existing `avalon_slave_0` address from 3 to 5 bits, and adds two new register banks to `go_peripheral.sv`. **No new Qsys components.** The HPS-side software grows by one new translation unit (`hw_timer.{h,c}`) and small patches to `ai.{h,c}` and `go_main.c`.

---

## Architecture choice (vs. design doc)

The design doc's §7.5 promised an FPGA-side `cycles_count[31:16]` for performance reporting *inside* `mcts_accel`. Phase 9 instead puts the cycle counter inside `go_peripheral` proper so the timer is available to *any* code path — not just MCTS. This lets us time the SW tree path too, which is exactly what makes the HW/SW comparison meaningful. The MCTS accelerator can still report its own internal counter via the existing `MCTS_STATUS` register; the Phase 9 timer is the outer wall-clock around `ai_get_move()`.

The heat-map tilemap was *not* in the original design doc. It's a parallel tilemap (81 cells × 4 bits, shape mirrored from `board_mem.sv`) that the renderer consults when overlay is enabled.

---

## What changed in this commit

### Hardware
- **`hw/heat_map.sv`** — new 81 × 4-bit tilemap with synchronous write, combinational read, sync clear-all. Inferred as LUT-RAM (324 bits — well under M10K threshold).
- **`hw/go_peripheral.sv`** — widened. Specifically:
  - `address` port: 3 → 5 bits.
  - Localparams added for `REG_HEAT_IDX` (0x08), `REG_HEAT_VAL` (0x09), `REG_OVERLAY_EN` (0x0A), `REG_HEAT_CLEAR` (0x0B), `REG_TIMER_START` (0x10), `REG_TIMER_STOP` (0x11), `REG_TIMER_D0..D3` (0x12..0x15), `REG_TIMER_LIVE0` (0x16).
  - New signals: `heat_idx_latch`, `hm_write_en`, `hm_clear_all`, `overlay_en`, `cycles_count`, `t_start`, `t_delta_latched`.
  - Decoder `always_comb` extended with cases for new regs.
  - `readdata` `always_comb` refactored to a `unique case` mux over the readable registers.
  - State-register `always_ff` extended with `heat_idx_latch` and `overlay_en`.
  - New `always_ff` runs a 32-bit free-running cycle counter at clk_50; `t_start` latches on TIMER_START, and `t_delta_latched = cycles_count − t_start` latches on TIMER_STOP.
  - `heat_map` instantiated next to `board_mem`, sharing `cell_idx` for its read address.
  - New 16-entry heat palette `heat_rgb` combinational block.
  - Color `always_comb` updated: empty-cell branch now consults `overlay_en && heat_value != 0` and applies `heat_rgb` if so.
- **`hw/go_peripheral_hw.tcl`** — `avalon_slave_0` address port width 3 → 5; `heat_map.sv` added to the synthesis fileset.

### Software
- **`sw/hw_timer.{h,c}`** — new 4-function wrapper (`hw_timer_init`, `hw_timer_start`, `hw_timer_stop_cycles`, `hw_timer_live_byte`) over the new timer registers. `cycles_to_ms` / `cycles_to_us` helpers for the display path.
- **`sw/ai.h`** — adds `ai_toggle_backend()`, `ai_backend_is_hw()`, `ai_backend_label()` over the existing `g_use_hw_mcts` flag; adds `MctsHeatCallback` typedef and `ai_set_mcts_heat_callback()` setter.
- **`sw/ai.c`** — implements those four functions; adds `publish_heat(root)` and calls it every 8 sims (plus once after the loop) inside `move_mcts`. No rename of `move_mcts` — the existing dispatch via `g_use_hw_mcts ? move_mcts_flat_hw : move_mcts` is preserved.
- **`sw/go_main.c`** — adds:
  - `REG_HEAT_*` defines.
  - `hw_set_heat`, `hw_overlay_enable`, `hw_heat_clear` helpers and the `heat_cb` MctsHeatCallback implementation that maps win-rate to a 0..15 heat level per cell.
  - `KEY_TAB` (0x2B) and `KEY_Y` (0x1C) HID keycodes.
  - State globals: `last_ai_cycles`, `last_ai_label`, `prev_ai_cycles`, `prev_ai_label`, `replay_snapshot`, `replay_valid`.
  - `hw_timer_init` + `ai_set_mcts_heat_callback(heat_cb)` once at startup.
  - `enter_game` clears heat, disables overlay, invalidates replay state.
  - `render_panel` adds Row 3 (y=41) with "BACKEND XX LAST X.XX MS  PREV YY Y.YY MS  Nx" and shrinks Row 4 (y=50) controls hint to include "TAB SWAP" and "Y REPLAY".
  - `apply_ai_move` now: snapshots `replay_snapshot` before mutating; rolls last→prev; calls `hw_heat_clear() + hw_overlay_enable(1)` before, and `hw_overlay_enable(0)` after the AI call; wraps the AI call with `hw_timer_start()` / `hw_timer_stop_cycles()`; copies `ai_backend_label()` into `last_ai_label`; prints the timing to stdout.
  - KEY_TAB handler: calls `ai_toggle_backend()` and re-renders the panel.
  - KEY_Y handler: copies `replay_snapshot` back into `b`, invalidates `replay_valid`, calls `ai_toggle_backend()`, re-pushes the board to FPGA, and re-issues `apply_ai_move()`.

### Updated register map

| Off  | Name           | R/W | Effect |
|------|----------------|-----|--------|
| 0x00 | SET_BLACK      | W   | as before |
| 0x01 | SET_WHITE      | W   | as before |
| 0x02 | CLEAR_CELL     | W   | as before |
| 0x03 | RESET_BOARD    | W   | as before |
| 0x04 | CURSOR         | W   | as before |
| 0x05 | STRIP_SWAP     | W   | as before |
| 0x06 | AUDIO_CMD      | W   | as before |
| 0x07 | AUDIO_STATUS   | R   | as before |
| 0x08 | HEAT_IDX       | W   | data[6:0] = cell index for the next HEAT_VAL write |
| 0x09 | HEAT_VAL       | W   | data[3:0] = heat value; commits at cell HEAT_IDX |
| 0x0A | OVERLAY_EN     | W   | data[0] = 1 enable overlay, 0 disable |
| 0x0B | HEAT_CLEAR     | W   | any = clear all 81 heat cells to 0 |
| 0x10 | TIMER_START    | W   | any = latch cycles_count → t_start |
| 0x11 | TIMER_STOP     | W   | any = latch (cycles_count − t_start) → delta |
| 0x12 | TIMER_D0       | R   | delta[7:0] |
| 0x13 | TIMER_D1       | R   | delta[15:8] |
| 0x14 | TIMER_D2       | R   | delta[23:16] |
| 0x15 | TIMER_D3       | R   | delta[31:24] |
| 0x16 | TIMER_LIVE0    | R   | cycles_count[7:0]  (sanity-check: counter ticks) |

---

## Step 1 — Re-import the Qsys IP (because `_hw.tcl` changed the address width)

In Platform Designer:
1. **Remove** existing `go_peripheral_0`.
2. **Re-add** it from IP Catalog. Component Editor preview should now show `avalon_slave_0` with a **5-bit `address`** port (previously 3-bit).
3. Re-wire as before. The strip slave (`avalon_slave_1`), VGA conduit, and audio conduit are unchanged.
4. **System → Address Map.** Confirm:
   - `avalon_slave_0` is at byte offset `0x0000` and now has span **32 bytes** (was 8).
   - `avalon_slave_1` is at byte offset `0x10000` and span 38,400 bytes.
   - There is no overlap. (There's a 64 KB gap between them; nothing to worry about.)
5. Save. Generate HDL.

If the regenerated `soc_system/synthesis/soc_system.v` doesn't show a 5-bit address path into `go_peripheral_0`, the re-import didn't pick up the new `_hw.tcl`. Re-check that the file you edited is the one in the same directory as `soc_system.qsys` (or wherever `IP_SEARCH_PATHS` resolves to).

## Step 2 — Build

```bash
cd hw/
make qsys      # if it isn't already up to date
make quartus   # adds ~120 ALMs + ~70 ALMs over Phase 8; trivial timing impact
make rbf
```

```bash
cd sw/
make go_main   # Makefile already lists hw_timer.c — no edit needed
```

## Step 3 — Smoke test (no UI changes needed)

The first test is whether the new registers are wired up at all. From the DE1-SoC shell:

```bash
sudo ./go_self_test probe         # existing test — must still pass
sudo ./go_main 3                  # PvC level 3 (MCTS), skip menus
```

Expected:
- Splash → game appears as before.
- Strip should immediately show a "BACKEND HW" or "BACKEND SW" line at y=41 (it depends on whether the FPGA MCTS accelerator probe succeeded in `ai_init`).
- Make a move with Enter. The AI plays. The strip's Row 3 should now read e.g. "BACKEND HW  LAST 1.30 MS". Stdout should print the same: `AI (White) plays (4,4).  [HW, 1.30 ms]`.

## Step 4 — Heat-map test

Force the SW tree path so heat is visible:

```bash
sudo ./go_main 3
# Make a move. After AI responds, press Tab.
# Strip should change to "BACKEND SW" (or HW if it was SW).
# Make another move; AI thinks 1–3 seconds. Empty cells should glow
# on a red→green spectrum during that time and clear when AI commits.
```

If heat shows during *both* SW and HW: `g_use_hw_mcts` is being ignored somewhere, or the HW dispatcher is silently routing to the SW eval. (That's actually fine functionally — `ai_init` falls back to SW eval when `hw_init_mcts()` returns -1.) The HW flat path *also* runs in SW under the hood when the FPGA accelerator isn't responding, in which case timings will look similar.

If heat *never* shows: see Troubleshooting below.

## Step 5 — Replay-on-toggle test

```bash
sudo ./go_main 3
# Make a move. Watch the AI play.
# Press Y. The AI rewinds to the same board, toggles the backend,
# and plays again. Watch the strip — it should now show
# "HW 1.30MS  PREV SW 1850.00MS  1420X" (or vice versa).
```

The AI's chosen move may or may not be the same as the original — they're different algorithms (tree vs flat) with different RNG seeds.

---

## Validation checklist

- [ ] Quartus synthesis succeeds. No new timing violations.
- [ ] `go_self_test probe` still reports MCTS accelerator OK (or its previous fail mode — Phase 9 doesn't touch the MCTS RTL).
- [ ] On the DE1-SoC, `sudo ./go_main 0` (PvP) plays exactly as before. No heat, no backend line, no timing line. (The "BACKEND SW" line will still appear at y=41 because PvP shares the same `render_panel` — that's fine; just confirm it doesn't disrupt the existing layout.)
- [ ] `sudo ./go_main 3` lights up the BACKEND line.
- [ ] Empty cells inside the board area tint red→green during the SW MCTS turn (~1-2 seconds).
- [ ] Empty cells return to plain burlywood as soon as the AI commits.
- [ ] After at least one AI move, the strip shows a `LAST X.XX MS` reading.
- [ ] Tab toggles "BACKEND SW" ⇄ "BACKEND HW" on stdout AND on the strip.
- [ ] Y rewinds and re-plays. After Y, the strip shows the speedup pair.
- [ ] `cat /sys/class/fpga_manager/fpga0/state` (or `dmesg | tail`) reports no DMA / Avalon errors during a 5-minute play session.

## Resource impact estimate (revise after first synthesis)

| Block                          | ALMs (Phase 9 only) | Notes |
|--------------------------------|---------------------|-------|
| `heat_map` 81 × 4-bit array    | ~40                 | LUT-RAM |
| Heat palette case (16 entries) | ~50                 | combinational |
| Avalon decode for 11 new regs  | ~80                 | with widened address |
| Timer (32b counter + 2 latches + mux) | ~70          | |
| Renderer empty-cell branch tweak | ~10               | one extra mux level |
| **Total**                      | **~250 ALMs**       | ~0.8 % of 5CSEMA5 |

LE / BRAM both unchanged (no M10K). No new DSP blocks.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Quartus error: "port `address` width mismatch" between Qsys and go_peripheral | Stale Qsys IP — wasn't re-imported after `_hw.tcl` widening. | Remove & re-add `go_peripheral_0` in Platform Designer. |
| Quartus warning: "unique case may not be fully specified" inside the new readdata or decoder always_comb | Some address values not listed but a default catches them — this is the intended pattern, the warning is just noting that not every 5-bit value has an explicit case. | Safe to ignore. If it bothers, switch `unique case` → `case`. |
| Build OK, but `./go_main 3` segfaults on the first AI move | `hw_timer_init` was never called, so the static `regs` pointer is NULL. | Verify the `hw_timer_init((volatile uint8_t *)go_regs);` line runs *after* `hw_init()` succeeds. |
| BACKEND line appears but always says "SW" even after pressing Tab | `ai_init` was never called, or you're running an old binary. Stdout prints `Phase 9: AI backend = ...` at startup — if missing, `main()` didn't reach the init block. | Rebuild; ensure `ai_init();` runs after `ai_seed()`. |
| `LAST` reads suspiciously small (e.g. 0.02 ms) for a SW MCTS turn that obviously took seconds | 32-bit counter wrapped (>85.9 s elapsed) — unlikely for MCTS at 200 sims; or the timer was started inside a different burst and the start/stop ordering interleaved with a system call. | If reproducible: instrument with `hw_timer_live_byte()` before/after to confirm the counter advances. |
| `LAST` reads as huge garbage (millions of ms) | `t_delta_latched = cycles_count - t_start` underflowed because TIMER_STOP fired before TIMER_START captured. | Confirm the Avalon write to TIMER_START completes before the AI call by adding a register *read* (e.g. `hw_read(REG_VGA_STATUS)`) as a fence between them — currently we trust the in-order LW bus. |
| Heat overlay appears but only shows the FIRST cell each turn (others stay neutral) | The HEAT_IDX latch isn't being re-written between cells. | Inspect `hw_set_heat` in go_main.c — must do *both* writes per cell. Don't bulk-write only HEAT_VAL. |
| Heat overlay tints non-empty cells (stones get colored backgrounds) | Renderer branch ordering wrong; heat case must only fire for `cell_value == 2'b00`. | Check `go_peripheral.sv` color block: heat branch should be `else if (cell_value == 2'b00 && overlay_en && heat_value != 4'd0)`. Branches above it (stones, grid, cursor) must take precedence. |
| Heat overlay never appears even when SW MCTS is clearly running (stdout prints sim progress) | Three suspects: (1) `ai_set_mcts_heat_callback(heat_cb)` not called; (2) `hw_overlay_enable(1)` not called in `apply_ai_move`; (3) tree MCTS finishes before the first 8-sim publish (would only happen if MCTS_SIMS < 8). | Trace each: add a `printf("publish\n")` to `publish_heat`, then check that `OVERLAY_EN` register reads back as 1 (would need an R-mux entry; easier: scope/SignalTap the `overlay_en` reg in `go_peripheral.sv`). |
| Heat overlay color values look washed out or wrong | Monitor gamma. The palette was tuned against an sRGB display. | Tweak the entries in the `heat_rgb` case inside `go_peripheral.sv` — they're just immediate constants. |
| Tab key doesn't toggle anything | Either: PvC + Level 3 isn't active (Tab is gated on both — check `pvc && ai_level == AI_MCTS`); or the keyboard's Tab keycode isn't 0x2B (some boards send 0x2C — check `references/_starter/lab2-sw` or print pkt.keycode[0] on Tab press). | If keycode differs, edit `KEY_TAB` define. |
| Y key replays but the speedup line never shows | The replay used the *same* backend label (e.g. Tab was pressed twice; or the original AI move was during a backend swap that happened before the next move). Check `prev_ai_label` and `last_ai_label` differ after the Y press. | The strip only shows the speedup row when labels differ — that's by design. |
| `make` complains "ai_hw.h: No such file" while building go_main.c | Phase 7b files weren't committed/pulled. | Either pull the Phase 7b commit, or temporarily comment out the `#include "ai_hw.h"` line in go_main.c and stub `ai_init()` to a no-op (heat overlay & timer still work; just no HW MCTS to toggle into). |
| Heat writes during MCTS cause visible flicker on the *board* (not the strip) | Avalon writes are not vblank-aligned. The heat reads in the renderer get one cycle of pipeline latency. Cells whose write straddles a scanout cycle may show stale value for one frame. | Functionally harmless — looks like the heat is "shimmering" which actually reads as MCTS activity. If it bothers, gate heat writes on `VGA_VS` rising edge by polling a VS status bit (add reg if needed) — for Phase 9 we accept the shimmer. |

## What's NOT in Phase 9

- Heat updates from the *HW flat MCTS* path. The HW flat call returns in ~1 ms, well under one frame, so live updates aren't practical. The HW path gets a brief flash of the final heat just before the move commits.
- Per-AI-move history beyond LAST/PREV (e.g. a graph of times over the game). The strip is 60 px tall; we'd need a different visualization for time-series.
- A keyboard shortcut to disable/enable the overlay independently from MCTS thinking — overlay is bound to the AI call. Easy to add if wanted.
- Heat-map storage to/from disk for replay analysis.

## Phase 9 → Phase 10 transition

Phase 10 (if pursued) would add a third interesting visualization: a **mini MCTS tree view** in the strip — top-3 candidate moves with their visit counts and win rates, updated live. The data is already in `root->children[]` — `publish_heat` could be extended to also publish a top-3 summary to a new register block, with the strip renderer drawing it as text.

Beyond Phase 10: pattern-based playout policy inside `rollout_engine.sv` (the design-doc-level §7.5 win). That's a substantial RTL change; defer until Phase 9 + 7b are both stable.
