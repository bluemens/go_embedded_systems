# HEAD Bring-Up — Build the Full Project at Once

Use this doc if you've decided to skip the strictly-phased approach and bring up everything in one go. It consolidates the HW work that's otherwise scattered across `PHASE1.md`, `PHASE2.md`, `PHASE5.md`, and `PHASE6.md` (the four phases that touched hardware).

After this is working, all software phases (3, 4, 7a, 8) just need `gcc` on the target — no FPGA rebuild.

## Prerequisites

- Linux/Windows machine with **Quartus Prime 21.1** (Mudd 1235 lab machines).
- **DE1-SoC board**, USB cables, VGA monitor, **wired USB keyboard** (boot-protocol HID; gaming keyboards may not work), powered speakers / headphones for line-out.
- **lab2 SD card** image already prepared (this is the boot image with u-boot + Linux that loads `soc_system.rbf` on power-on). If you don't have one, follow `references/_starter/lab2.pdf` first.
- This repo cloned to a known path on the class machine.

## Overview of what we're about to do

1. Open Platform Designer, add `go_peripheral_0` with two Avalon slaves and two conduits (VGA + audio).
2. Set base addresses (slave_0 at 0x0000, slave_1 at 0x10000).
3. Generate HDL.
4. Add the `codec_interface/` Altera-UP audio bundle to the Quartus project (.qsf).
5. Make sure the four `.vh` audio sample files are on the synthesis search path.
6. Patch `soc_system_top.sv`: wire VGA + audio conduits out, instantiate `codec_interface`, remove dummy assigns.
7. `make qsys && make quartus && make rbf`.
8. Copy `.rbf` to SD card, boot.
9. Build software on the target. Run progressively (`go_test` → `go_phase3` → `go_main`).

Total time: ~5 min Quartus compile, ~10 min Platform Designer setup, plus board boot. Plan one ~30-min sitting on the class machine.

---

## Step 1 — Open Platform Designer

From a terminal in `hw/`:

```bash
quartus_sh -t soc_system.tcl    # creates .qpf/.qsf if not already present
```

Open Quartus, open `soc_system.qpf`, then **Tools → Platform Designer**, then **File → Open** `soc_system.qsys`.

## Step 2 — Make `go_peripheral` discoverable

The IP Catalog needs to find our component. If "Go Peripheral" doesn't appear when searching:

1. **Tools → Options → IP Search Path** → Add the absolute path to `<repo>/hw/`.
2. Click **Refresh Component Library**.
3. Search again — "Go Peripheral" should appear under "Project". If not, check that `hw/go_peripheral_hw.tcl` exists and is syntactically valid (open it with a text editor; should start with `package require -exact qsys 16.1`).

## Step 3 — Add and configure `go_peripheral_0`

Double-click "Go Peripheral" in the IP Catalog → instance name `go_peripheral_0` → **Finish**. The Component Editor preview should show:

- `clock` (clock sink)
- `reset` (reset sink)
- `avalon_slave_0` — register interface (8-bit data, 3-bit byte address)
- `avalon_slave_1` — strip framebuffer (32-bit data, 14-bit word address)
- `vga` — conduit (8-bit R/G/B + control signals)
- `audio` — conduit (24-bit dac_left/right out, 1-bit advance in)

**If only some of these appear**, the IP cache may be stale. Close Platform Designer, run:
```bash
rm -rf ~/.altera.qsys-pro/component_cache.json    # may not exist; harmless
```
Reopen Platform Designer.

## Step 4 — Wire connections in System Contents

For each go_peripheral_0 port, drag a connection (or right-click → Connect):

| go_peripheral_0 port | Connect to |
|----------------------|------------|
| `clock` | `clk_0.clk` |
| `reset` | `clk_0.clk_reset` |
| `avalon_slave_0` | `hps_0.h2f_lw_axi_master` |
| `avalon_slave_1` | `hps_0.h2f_lw_axi_master` (same master, different base) |
| `vga` | **export** (double-click in the Export column; accept default name "vga") |
| `audio` | **export** (default name "audio") |

## Step 5 — Set base addresses

**System → Address Map** (or right-click each slave's Base column).

| Slave | Base address (within LW bridge, byte offset) |
|-------|----------------------------------------------|
| `go_peripheral_0.avalon_slave_0` | `0x00000000` |
| `go_peripheral_0.avalon_slave_1` | `0x00010000` |

These must match `GO_PERIPHERAL_OFFSET = 0x0000` and `STRIP_FB_OFFSET = 0x10000` in `sw/go_main.c`. If Qsys auto-assigned different values, **edit the .c constants to match** (or edit Qsys; the .c is easier).

## Step 6 — Save and Generate HDL

Save (`Ctrl+S`). Click **Generate HDL** (top-right) → choose **Verilog** → **Generate**. ~30 sec. Should finish with `Generation completed successfully`. Close Platform Designer.

This regenerates `soc_system/synthesis/soc_system.v` with all our exported ports.

## Step 7 — Add the `codec_interface/` bundle to .qsf

The 20 files in `hw/codec_interface/` are Altera-UP's audio core. They live in the FPGA fabric (not in Qsys), so Quartus needs to know about them via the project file.

Easiest: open Quartus → **Project → Add/Remove Files in Project** → click **Add Files** → select all 20 files in `hw/codec_interface/` → OK.

Manual alternative — append to `hw/soc_system.qsf`:

```tcl
set_global_assignment -name SEARCH_PATH "codec_interface"
set_global_assignment -name SYSTEMVERILOG_FILE codec_interface/codec_interface.sv
set_global_assignment -name VERILOG_FILE codec_interface/audio_codec.v
set_global_assignment -name VERILOG_FILE codec_interface/audio_and_video_config.v
set_global_assignment -name VERILOG_FILE codec_interface/clock_generator.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_Audio_Bit_Counter.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_Audio_In_Deserializer.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_Audio_Out_Serializer.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_Clock_Edge.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_I2C.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_I2C_AV_Auto_Initialize.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_I2C_DC_Auto_Initialize.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_I2C_LCM_Auto_Initialize.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_Slow_Clock_Generator.v
set_global_assignment -name VERILOG_FILE codec_interface/Altera_UP_SYNC_FIFO.v
set_global_assignment -name VERILOG_FILE codec_interface/xck_generator.v
set_global_assignment -name QIP_FILE codec_interface/xck_generator/xck_generator_0002.qip
```

The `.qip` file pulls in the actual altPLL hardware that generates the WM8731 master clock.

## Step 8 — Make sure `.vh` files are findable

The four placeholder audio ROMs (`place.vh` / `capture.vh` / `illegal.vh` / `gameover.vh` in `hw/`) are loaded by `$readmemh` at synthesis. They should be picked up automatically because `go_peripheral_hw.tcl` lists them as fileset entries — but if synthesis later complains "could not open file place.vh", append to `hw/soc_system.qsf`:

```tcl
set_global_assignment -name MISC_FILE place.vh
set_global_assignment -name MISC_FILE capture.vh
set_global_assignment -name MISC_FILE illegal.vh
set_global_assignment -name MISC_FILE gameover.vh
```

Or simpler workaround at compile time:
```bash
cp *.vh output_files/ 2>/dev/null
```

## Step 9 — Patch `soc_system_top.sv`

Three edits to `hw/soc_system_top.sv`. **Do all three before compiling**, or you'll get pin-driver conflicts.

### 9a. Add VGA + audio wires to the `soc_system soc_system0(...)` instantiation

Find the line near the bottom of the `soc_system soc_system0(...)` port list:
```systemverilog
     .hps_hps_io_gpio_inst_GPIO61  ( HPS_GSENSOR_INT )
```

Add a comma to it and append:
```systemverilog
     .hps_hps_io_gpio_inst_GPIO61  ( HPS_GSENSOR_INT ),

     // Phase 1 + Phase 5: VGA conduit
     .vga_r       (VGA_R),
     .vga_g       (VGA_G),
     .vga_b       (VGA_B),
     .vga_clk     (VGA_CLK),
     .vga_hs      (VGA_HS),
     .vga_vs      (VGA_VS),
     .vga_blank_n (VGA_BLANK_N),
     .vga_sync_n  (VGA_SYNC_N),

     // Phase 6: audio conduit (wired to codec_interface below)
     .audio_dac_left  (aud_dac_left),
     .audio_dac_right (aud_dac_right),
     .audio_advance   (aud_advance)
```

After Generate HDL in Step 6, verify exact port names by `grep "vga_\|audio_" soc_system/synthesis/soc_system.v | head -20`. They should be `vga_r`, `vga_g`, etc., and `audio_dac_left`, etc. If Qsys named them differently (e.g., `vga_export_r`), match the generated names here.

### 9b. Add codec_interface instantiation

After the `soc_system soc_system0(...);` block but before `endmodule`, add:

```systemverilog
   /* Phase 6: Audio CODEC wrapper (Altera-UP, sits at toplevel, NOT in Qsys).
    * codec_interface drives AUD_*/FPGA_I2C_* board pins directly and exposes
    * a 48 kHz sample-rate "advance" strobe to our audio_controller. */
   wire [23:0] aud_dac_left, aud_dac_right;
   wire [23:0] aud_adc_left_unused, aud_adc_right_unused;
   wire        aud_advance;

   codec_interface codec_inst (
       .CLOCK_50      (CLOCK_50),
       .reset         (1'b0),         // codec auto-inits via I²C
       .dac_left      (aud_dac_left),
       .dac_right     (aud_dac_right),
       .adc_left      (aud_adc_left_unused),
       .adc_right     (aud_adc_right_unused),
       .advance       (aud_advance),
       .FPGA_I2C_SCLK (FPGA_I2C_SCLK),
       .FPGA_I2C_SDAT (FPGA_I2C_SDAT),
       .AUD_XCK       (AUD_XCK),
       .AUD_DACLRCK   (AUD_DACLRCK),
       .AUD_ADCLRCK   (AUD_ADCLRCK),
       .AUD_BCLK      (AUD_BCLK),
       .AUD_ADCDAT    (AUD_ADCDAT),
       .AUD_DACDAT    (AUD_DACDAT)
   );
```

### 9c. Remove the dummy assigns for VGA, AUD_*, FPGA_I2C_*

In the lab3-baseline `soc_system_top.sv` (around lines 275–319), find and **delete** these lines:

```systemverilog
   assign AUD_ADCLRCK = SW[1] ? SW[0] : 1'bZ;
   assign AUD_BCLK    = SW[1] ? SW[0] : 1'bZ;
   assign AUD_DACDAT  = SW[0];
   assign AUD_DACLRCK = SW[1] ? SW[0] : 1'bZ;
   assign AUD_XCK     = SW[0];
```

```systemverilog
   assign FPGA_I2C_SCLK = SW[0];
   assign FPGA_I2C_SDAT = SW[1] ? SW[0] : 1'bZ;
```

```systemverilog
   assign {VGA_R, VGA_G, VGA_B} = { 24{ SW[0] } };
   assign {VGA_BLANK_N, VGA_CLK,
           VGA_HS, VGA_SYNC_N, VGA_VS} = { 5{ SW[0] } };
```

**Keep** the dummy assigns for everything else (DRAM_*, GPIO, HEX, IRDA, LEDR, PS2, TD, ADC) — those signals aren't driven by anything we built, and Quartus needs *some* driver to avoid synthesis warnings.

## Step 10 — Compile

```bash
cd hw/
make qsys      # ~30 s — regenerates HDL after our Qsys edits
make quartus   # ~5-7 min — full synthesis to soc_system.sof
make rbf       # ~10 s — converts to .rbf for SD card
```

If any step fails, see "Common HEAD-state issues" at the end.

## Step 11 — Boot

Copy `output_files/soc_system.rbf` to the FAT partition of your SD card (overwriting whatever was there from lab2). The lab2 u-boot will program the FPGA from this file on boot.

Insert SD card. Power on. Plug in:
- VGA monitor (D-SUB, both ends)
- USB keyboard (either Type-A port)
- Speakers / headphones to **line-out** (NOT mic-in)

Wait for Linux to boot — you'll see kernel messages on UART (use `screen /dev/ttyUSB0 115200` from your laptop), and eventually a login prompt.

## Step 12 — Build software (on the DE1-SoC)

Log in as `root` (no password on the lab2 image, IIRC). Then:

```bash
cd /path/to/sw       # wherever you placed sw/ on the SD card or via scp
apt-get install libusb-1.0-0-dev    # if not already installed
make go_main         # ignore any errors about kernel module / vga_ball.ko —
                     # we're using the userspace mmap path, not the kernel module
```

(The `default: module hello go_test go_phase3 go_main` in the Makefile tries to build the lab3 kernel module. If that fails, just `make go_main` directly — it skips the module.)

## Step 13 — Test progressively

Even though the bitstream supports everything, validate phase by phase using the bring-up programs we kept around:

```bash
# Phase 2 — board + tilemap. No keyboard/audio yet.
sudo ./go_test
#   Expected: board appears, then 5 stones at hoshi (B/W/B/W/B), then they vanish.

# Phase 3 — cursor + USB keyboard.
sudo ./go_phase3
#   Expected: green ring at tengen. Arrows move it. Enter places alternating
#   B/W stones. R resets. Esc quits.

# Phase 4-5-6 — full PvP with score panel and audio.
sudo ./go_main 0
#   Expected: live score panel at bottom, click sound on placement, distinguishable
#   audio for capture vs illegal vs game-over (well, all clicks for now since
#   the .vh files are placeholders).

# Phase 7a — AI levels.
sudo ./go_main 1     # random — instant
sudo ./go_main 2     # greedy — instant, surprisingly competent
sudo ./go_main 3     # MCTS 200 sims. **Time this**: it's the question whether
                     # we need Phase 7b. Per PHASE7.md, my back-of-envelope
                     # estimate is ~16 s/turn. If real numbers confirm that,
                     # we have a problem; if it's <2 s, we're fine.

# Phase 8 — full UI.
sudo ./go_main
#   Expected: Title screen → Enter → Mode select → ... → Game.
```

## Common HEAD-state issues

| Symptom | Likely cause + fix |
|---|---|
| **VGA black, monitor says "no signal"** | Qsys vga conduit not exported, or Step 9a port names don't match. Check `grep "vga_" soc_system/synthesis/soc_system.v` to see actual exported port names; match them in soc_system_top.sv. |
| **VGA shows colors but they're scrambled** | RGB channel order wrong, or a pin assignment got nuked. Check `output_files/soc_system.fit.rpt` for the VGA_R[0..7], VGA_G, VGA_B pins — they should match `[UM Table 3-16]` (PIN_A13, etc.). |
| **VGA OK, board renders, but cursor stuck at (0,0)** | Phase 3 wrote 0x80 (visible+idx 0). Check that REG_CURSOR write actually reaches the slave — `printf` after `hw_cursor()`. |
| **Phase 5 strip area is solid color** | `STRIP_FB_OFFSET` constant in go_main.c doesn't match Qsys avalon_slave_1 base. Verify both are 0x10000. |
| **Phase 5 strip flickers between contents** | Swap firing >1×/frame. `swap_armed` should clear inside `strip_fb.sv` at the vsync_pulse — verify. |
| **Audio: no sound at all** | (a) `HPS_I2C_CONTROL` got pulled high somewhere — search the toplevel for it and remove any drive. (b) codec_interface not instantiated (Step 9b missed). (c) AUD_* pins still tied to dummy assigns (Step 9c missed — search for `assign AUD_` and confirm none remain). |
| **Audio: continuous click while idle** | `audio_controller.sv`'s idle-state branch isn't driving dac_left/right to 0. Likely a typo. |
| **`mmap: Permission denied` at runtime** | Not running as root. `sudo ./go_main`. |
| **`No USB keyboard found.`** | Keyboard isn't a boot-protocol HID (some gaming keyboards aren't). Try a basic wired keyboard. |
| **Phase 7a MCTS hangs for >5 s with no output** | Working as expected if estimate is right. Add `clock_gettime` instrumentation around `apply_ai_move` to measure. If unacceptable, drop `MCTS_SIMS` from 200 → 100 in `ai.c`. |
| **`$readmemh: file not found place.vh` during synthesis** | Step 8 wasn't done. Copy `*.vh` to `output_files/` or add MISC_FILE entries to .qsf. |
| **Pin conflicts at synthesis: "VGA_R[0] has multiple drivers"** | Step 9c not fully done — dummy `assign {VGA_R, VGA_G, VGA_B} = ...` still present. Remove it. |
| **`hw/Makefile`'s `make module` fails** | We're not using the kernel module path. Just `make go_main` directly. |

## After this works

You have a functioning game. Open questions:
1. **Phase 7a latency** — measure on real hardware. If MCTS is too slow, decide whether to drop sims or build Phase 7b.
2. **Real audio samples** — replace placeholder .vh files using the Autotune WAV-to-hex pipeline (see `hw/PHASE6.md` "Producing real samples").

If something goes wrong and you need to bisect: `git checkout 61e0c52` (Phase 1) gives you the minimum starting point. Each PHASE-N.md has its own "what changed in this commit" so you can selectively re-apply edits.
