# Phase 6 — Audio (Sound Effects)

**Goal:** plug a speaker into the DE1-SoC's line-out and hear short clicks / whooshes / chimes for stone placement, captures, illegal moves, and game-over. Four ROM-stored sound effects, played by an Avalon-driven audio_controller through a PianoHeroes-style codec_interface wrapper around the Altera UP audio core.

**Prerequisites:** Phases 1–5 working. Phase 6 adds a new HDL file (`audio_controller.sv`), a vendor bundle (`codec_interface/`, 200 KB of Altera UP files), four placeholder `.vh` sample ROMs, and one new toplevel module instantiation. **No new Avalon slaves**, just two more registers on the existing `avalon_slave_0`.

## Architectural choice (vs. design doc §5)

The design doc envisioned the audio core inside Qsys with Avalon-ST. We're following PianoHeroes instead: the **codec_interface module sits in the toplevel** (NOT in Qsys) and drives `AUD_*` / `FPGA_I2C_*` board pins directly. Our `go_peripheral` exposes 24-bit `dac_left` / `dac_right` outputs and an `advance` input through a Qsys conduit; the toplevel wires them to the codec_interface instance. This skips a chunk of Qsys integration work that PianoHeroes already proved out and bypasses the I²C MUX issue (`HPS_I2C_CONTROL` stays at FPGA-owned).

## What changed in this commit

- **`hw/codec_interface/`** — Altera UP audio bundle, copied verbatim from `references/PianoHeroes/source/project_hw/Audio/codec_interface/` (20 files, ~200 KB). This is the I²S serializer + I²C auto-init for the WM8731. Black box; we don't edit it.
- **`hw/audio_controller.sv`** — new ~120-line module:
  - 4 ROM banks loaded via `$readmemh` from `place.vh` / `capture.vh` / `illegal.vh` / `gameover.vh`.
  - 8 kHz sample rate; each ROM sample is held for 6 `advance` pulses to upsample to the codec_interface's native 48 kHz (zero-order hold; quality is fine for short SFX).
  - State machine: idle → playing on AUDIO_CMD write, returns to idle when sample_idx hits ROM length.
  - 16-bit signed → 24-bit signed sign-extension to drive dac_left/right.
- **`hw/go_peripheral.sv`** — added:
  - `dac_left`, `dac_right` outputs (24-bit), `advance` input.
  - `readdata` output (8-bit) for AUDIO_STATUS reads.
  - `REG_AUDIO_CMD` (0x06) write-trigger logic, `REG_AUDIO_STATUS` (0x07) read returning `{7'b0, busy}`.
  - `audio_controller` instantiation.
- **`hw/go_peripheral_hw.tcl`** — added `audio_controller.sv` + 4 `.vh` files to the synthesis fileset; declared `readdata` on `avalon_slave_0`; added new `audio` conduit interface with `dac_left` / `dac_right` / `advance`.
- **`hw/{place,capture,illegal,gameover}.vh`** — placeholder sample ROMs: 8 samples of a brief click pattern at the start, zeros for the rest. Replace with real audio in Phase 6b (see "Producing real samples" below).
- **`sw/go_main.c`** — `hw_play_audio()` helper, called on PLACE / CAPTURE (`captured_*` increment detection) / ILLEGAL / GAME_OVER events.

## Updated register map

| Off  | Name           | R/W | Effect |
|------|----------------|-----|--------|
| 0x00 | SET_BLACK      | W   | as before |
| 0x01 | SET_WHITE      | W   | as before |
| 0x02 | CLEAR_CELL     | W   | as before |
| 0x03 | RESET_BOARD    | W   | as before |
| 0x04 | CURSOR         | W   | as before |
| 0x05 | STRIP_SWAP     | W   | as before |
| 0x06 | AUDIO_CMD      | W   | data[2:0]: 0=stop, 1=place, 2=capture, 3=illegal, 4=game_over |
| 0x07 | AUDIO_STATUS   | R   | bit 0: 1 = currently playing |

## Step 1 — Re-import the Qsys IP (because `_hw.tcl` got new ports + a conduit)

In Platform Designer:
1. Remove existing `go_peripheral_0`.
2. Re-add it from IP Catalog. Component Editor preview should now show:
   - `clock`, `reset` (sinks)
   - `avalon_slave_0` (with `readdata` newly visible)
   - `avalon_slave_1` (strip)
   - `vga` conduit
   - **`audio` conduit** (new — `dac_left` / `dac_right` outs + `advance` in)
3. Re-wire as before. The new `audio` conduit just gets exported (double-click its Export column).
4. **System → Address Map**: confirm `avalon_slave_0` at 0x0000, `avalon_slave_1` at 0x10000.
5. Save. Generate HDL.

## Step 2 — Patch `soc_system_top.sv` to instantiate `codec_interface`

Add to the toplevel **after** the `soc_system soc_system0(...)` instantiation (the bottom half of the file, where the dummy assigns currently live for AUD_* and FPGA_I2C_*):

```systemverilog
   /* ─── Audio CODEC wrapper (Altera UP, sits at toplevel, not in Qsys) ─── */
   wire [23:0] aud_dac_left, aud_dac_right;
   wire        aud_advance;

   codec_interface codec_inst (
       .CLOCK_50    (CLOCK_50),
       .reset       (1'b0),                // tie low; codec auto-inits
       .dac_left    (aud_dac_left),
       .dac_right   (aud_dac_right),
       .adc_left    (),                    // unused (recording not supported)
       .adc_right   (),
       .advance     (aud_advance),
       .FPGA_I2C_SCLK (FPGA_I2C_SCLK),
       .FPGA_I2C_SDAT (FPGA_I2C_SDAT),
       .AUD_XCK     (AUD_XCK),
       .AUD_DACLRCK (AUD_DACLRCK),
       .AUD_ADCLRCK (AUD_ADCLRCK),
       .AUD_BCLK    (AUD_BCLK),
       .AUD_ADCDAT  (AUD_ADCDAT),
       .AUD_DACDAT  (AUD_DACDAT)
   );
```

Then add to the existing `soc_system soc_system0(...)` instantiation, after the VGA wires:

```systemverilog
     .audio_dac_left  (aud_dac_left),
     .audio_dac_right (aud_dac_right),
     .audio_advance   (aud_advance),
```

(The exact port names will be `audio_dac_left` etc. because Qsys prepends the conduit interface name `audio` to each port. Verify after Generate HDL by reading the generated `soc_system/synthesis/soc_system.v`.)

**Remove these dummy assigns** (around lines 279–283 of the original lab3 toplevel):
```systemverilog
   assign AUD_ADCLRCK = SW[1] ? SW[0] : 1'bZ;
   assign AUD_BCLK    = SW[1] ? SW[0] : 1'bZ;
   assign AUD_DACDAT  = SW[0];
   assign AUD_DACLRCK = SW[1] ? SW[0] : 1'bZ;
   assign AUD_XCK     = SW[0];
```

And:
```systemverilog
   assign FPGA_I2C_SCLK = SW[0];
   assign FPGA_I2C_SDAT = SW[1] ? SW[0] : 1'bZ;
```

(`HPS_I2C_CONTROL` should stay at its existing dummy / wired state — keep it low so the FPGA owns the I²C bus.)

## Step 3 — Add the codec_interface bundle to Quartus

Quartus needs to know about the 20 files in `hw/codec_interface/`. Two options:

1. **Add to .qsf manually** — at the top of `soc_system.qsf` (after `make project` runs once), append:
   ```
   set_global_assignment -name SYSTEMVERILOG_FILE codec_interface/codec_interface.sv
   set_global_assignment -name VERILOG_FILE codec_interface/audio_codec.v
   set_global_assignment -name VERILOG_FILE codec_interface/audio_and_video_config.v
   set_global_assignment -name VERILOG_FILE codec_interface/clock_generator.v
   ... (all 20 files)
   ```
2. **Use a search path** — easier:
   ```
   set_global_assignment -name SEARCH_PATH "codec_interface"
   set_global_assignment -name SYSTEMVERILOG_FILE codec_interface/codec_interface.sv
   ```
   Quartus will pull in the rest as referenced.

Easiest in practice: open the project in Quartus GUI → Project → Add/Remove Files in Project → add the whole `codec_interface/` directory. Quartus rewrites the .qsf for you.

## Step 4 — Compile + program

```bash
cd hw/
make qsys
make quartus    # ~6-7 min — adds ~30 M10K of audio ROM + the codec serializer
make rbf
```

Plug a speaker / headphones into the DE1-SoC's **line-out** (NOT mic-in).

## Step 5 — Test

```bash
sudo ./go_main
```

Expected sounds:
- **Place a stone** (Enter on empty cell): brief click
- **Capture** (Enter that captures opponent stones): same click pattern (placeholder; real audio needs distinguishing whoosh)
- **Illegal move** (Enter on occupied / suicide / ko): same click (placeholder)
- **Two passes → game over**: same click

All four sound identical with the placeholder ROMs because they all start with the same 8-sample click pattern. Phase 6b replaces them with real WAV-derived samples.

## Producing real samples (Phase 6b — coming later)

The Autotune project's `references/Autotune/source/Autotune/PYTHON/wavfile_construction/` has the WAV → `.vh` script. Pipeline:

```bash
ffmpeg -i place.mp3 -ar 8000 -ac 1 -sample_fmt s16 -acodec pcm_s16le place.wav
python3 wavfile_construction/wav_to_hex.py place.wav > place.vh
# verify line count matches LEN_PLACE in audio_controller.sv (1600)
```

Sample sources can be from freesound.org (CC0) or recorded with a phone. Keep them short:
- place: 0.20s (light click)
- capture: 0.30s (deeper click + small whoosh)
- illegal: 0.15s (low buzz)
- game_over: 1.50s (chime, more elaborate)

If a real sample is shorter than the LEN constant, pad with zeros. If longer, the design doc constants must be updated and `audio_controller.sv` recompiled.

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| No sound at all | `HPS_I2C_CONTROL` is high (HPS owns I²C). Codec_interface can't auto-init. Tie it low in the toplevel. |
| Sound is super quiet | Check the WM8731 headphone volume in `audio_and_video_config.v`. Default should be reasonable; bump if needed. |
| Sound but distorted | 8 kHz → 48 kHz upsampling has aliasing. With the placeholder click pattern, this is fine; real audio may need a low-pass filter at 4 kHz before sampling. |
| Continuous click while idle | dac_left/right not being driven to 0 in idle state — check `audio_controller.sv` always_ff branch. |
| Can compile but `$readmemh` warns "no such file" | The `.vh` files aren't on Quartus's search path. Add them to the .qsf or copy into the synthesis dir. |
| AUDIO_STATUS reads always 0 | `readdata` not wired; check the `_hw.tcl` `add_interface_port avalon_slave_0 readdata readdata Output 8` line. |

## What's NOT in Phase 6

- Real audio samples (placeholder clicks only). Phase 6b.
- ADC / recording (codec_interface supports it but we tie ADC outputs to nothing).
- Per-key beeps for cursor movement (could add easily; not in design doc).
- Volume control (single hardcoded volume in audio_and_video_config.v).

## Phase 6 → Phase 7 transition

Phase 7 is the AI engine. Per design doc §7.5, the rollout phase becomes a hardware accelerator with N parallel uniform-random playout engines. That's substantial new HDL on top of what we've built. The CPU still runs the MCTS tree (selection / expansion / backpropagation) and hands off batches of root positions to the FPGA for rollouts.

Software prerequisites (start of Phase 7, all in HPS C):
- MCTS tree node struct (move, parent, children[], visits, wins)
- UCB1 child selection
- Tree expansion
- Backpropagation up parent chain
- Top-level `ai_get_move(BoardState *b, AiLevel level)` returning a `Move`

Hardware addition (later in Phase 7 if/when timing-critical):
- Rollout accelerator with bitboard ops, register-mapped to a third Avalon slave or an Avalon-MM master to read board state from BRAM.

Phase 7 is the largest remaining milestone. Recommend doing 7a (CPU-only AI: random + greedy + serial MCTS) first, validating playable strength, then 7b (FPGA accelerator) only if MCTS latency is unacceptable.
