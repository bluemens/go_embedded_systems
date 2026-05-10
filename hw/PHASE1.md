# Phase 1 — Static Empty Go Board on VGA

**Goal:** boot the DE1-SoC and see a static 9×9 board (burlywood squares + black grid + 5 hoshi dots, dark gray surround) on the VGA monitor. No game logic, no registers used yet. Just validates the pipeline.

**What's already in this repo (committed):**
- `go_peripheral.sv` — new module, replaces `vga_ball.sv` as the active VGA peripheral
- `go_peripheral_hw.tcl` — Qsys component descriptor

**What you need to do on a class machine** (Quartus 21.1 + Platform Designer):

## Step 1 — Open Platform Designer

```bash
cd hw/
quartus_pgmw                        # only if first time on this account, to validate Quartus
quartus_sh -t soc_system.tcl        # creates the .qpf/.qsf if not present
```

Open `soc_system.qsys` in Platform Designer (from Quartus: **Tools → Platform Designer**, then File → Open `soc_system.qsys`).

## Step 2 — Add the `go_peripheral` component to Qsys

1. **IP Catalog** pane (left): search for "go" → if it doesn't appear, add the path:
   - **Tools → Options → IP Search Path** → Add `<repo>/hw/`
   - Click **Refresh Component Library**
   - Search again. "Go Peripheral" should now appear under the project's custom IP.

2. **Add the component:**
   - Double-click "Go Peripheral" → instance name `go_peripheral_0` → click **Finish**.
   - It will appear in the System Contents pane.

3. **Wire its connections** (drag connections in the System Contents pane, or right-click connection points):

   | go_peripheral_0 port | Connect to |
   |----------------------|------------|
   | `clock` | `clk_0.clk` (the existing 50 MHz clock source) |
   | `reset` | `clk_0.clk_reset` |
   | `avalon_slave_0` | `hps_0.h2f_lw_axi_master` (the lightweight HPS-to-FPGA bridge) |
   | `vga` | (export — see step 4) |

4. **Export the VGA conduit:** in System Contents, find the `Export` column for `go_peripheral_0.vga` and double-click; type `vga` (or just press Enter to accept default). This makes the conduit visible at the top of the soc_system module.

5. **Assign base address:** in System Contents, find `go_peripheral_0.avalon_slave_0`. The Base column should auto-assign — click **System → Assign Base Addresses** if it shows `?`. Note the assigned address — should be at offset `0x00000000` from the LW bridge (i.e., physical `0xFF200000`).

6. **Save** (`Ctrl+S`).

7. **Generate HDL** (top-right "Generate HDL" button). Choose VERILOG. This regenerates `soc_system/synthesis/soc_system.v` with the new `vga_*` ports exposed at the top of the system.

## Step 3 — Patch `soc_system_top.sv`

Apply the following diff. This wires the Qsys-exported VGA conduit out to the physical pins, replacing the dummy assigns.

### Add to the `soc_system soc_system0(...)` instantiation (after the last HPS GPIO line)

Find this line near the end of the instantiation (around line 269):

```systemverilog
     .hps_hps_io_gpio_inst_GPIO61  ( HPS_GSENSOR_INT )
```

Change it (add a comma) and append the eight VGA connections:

```systemverilog
     .hps_hps_io_gpio_inst_GPIO61  ( HPS_GSENSOR_INT ),

     .vga_r       (VGA_R),
     .vga_g       (VGA_G),
     .vga_b       (VGA_B),
     .vga_clk     (VGA_CLK),
     .vga_hs      (VGA_HS),
     .vga_vs      (VGA_VS),
     .vga_blank_n (VGA_BLANK_N),
     .vga_sync_n  (VGA_SYNC_N)
```

### Remove the dummy VGA assigns

Find and **delete** these three lines (around 317–319):

```systemverilog
   assign {VGA_R, VGA_G, VGA_B} = { 24{ SW[0] } };
   assign {VGA_BLANK_N, VGA_CLK,
           VGA_HS, VGA_SYNC_N, VGA_VS} = { 5{ SW[0] } };
```

This pattern was confirmed against `references/FlappyBird/source/4840-Flappy-Bird/lab3-hw/soc_system_top.sv`, which made the identical diff.

## Step 4 — Compile and program

```bash
cd hw/
make qsys      # ~30s — regenerates HDL from the updated .qsys
make quartus   # ~3-5 min — full synthesis to soc_system.sof
make rbf       # ~5s — converts .sof to .rbf for the SD card
```

If any step fails:
- `make qsys` errors mean the `_hw.tcl` is wrong → reopen Platform Designer, re-import the IP, regenerate
- `make quartus` errors mean the toplevel patch is wrong, or pin assignments are off → check `output_files/*.rpt`
- Critical pin assignment errors → check `soc_system.qsf` references VGA pins per UM Table 3-16

## Step 5 — Boot and verify

Copy `output_files/soc_system.rbf` to the FAT partition of the SD card (lab2 boot image), reboot the DE1-SoC. The HPS will load Linux and ask u-boot to program the FPGA from this `.rbf`.

Expected output on the VGA monitor: dark gray surround, burlywood square in the middle of the screen with a 9×9 black grid and 5 small black dots at the hoshi positions. No animation, no input response.

## What if it doesn't work?

| Symptom | Likely cause |
|---------|--------------|
| Black screen | VGA conduit not connected or `VGA_BLANK_n` stuck low. Check `soc_system_top.sv` patch. |
| Solid color, no grid | `vga_counters` not running. Check `clk_0.clk` connection in Qsys. |
| Grid in wrong place | `BOARD_LEFT/TOP/CELL_PITCH` constants in `go_peripheral.sv` vs. design doc §4.1. |
| Sync issues / monitor "no signal" | `VGA_HS/VS/BLANK_n` not connected. Verify pin assignments per UM Table 3-16. |
| Compilation slow / fails timing | Synthesis-inferred dividers in `col = bx / CELL_PITCH`. Acceptable for Phase 1; we can optimize with a range table in Phase 2 if needed. |

## What we are NOT doing in Phase 1

- No HPS-side software changes (the HPS just boots Linux and does nothing — old `hello.c` won't have any visible effect since registers aren't actually used)
- No tilemap, no game logic, no audio, no keyboard input
- No double-buffering (no strip framebuffer yet)
- The Avalon slave accepts writes but ignores them — Phase 2 wires them up
- The old `vga_ball.sv` file is still in the repo; it's no longer instantiated. We'll delete it once `go_peripheral` works.
