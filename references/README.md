# References — Go on DE1-SoC

Curated reference material from CSEE 4840 Spring 2025 (Prof. Stephen Edwards), pulled from
<https://www.cs.columbia.edu/~sedwards/classes/2025/4840-spring/index.html>.

These projects share the same target platform (DE1-SoC, Cyclone V SoC, ARM HPS + FPGA) and the
same hardware/software architecture pattern (Avalon LW bridge, custom Verilog peripheral, Linux
userspace driver via `/dev/mem` + `mmap`) that our Go project uses. Each project here was
selected because it overlaps meaningfully with at least one subsystem in our design document.

## Layout

```
references/
├── _starter/                 Course starter material (labs, sample design doc, tile graphics)
│   ├── lab1/, lab1.pdf       Initial DE1-SoC bring-up
│   ├── lab2/, lab2.pdf       USB keyboard via libusb
│   ├── lab3-hw/, lab3-sw/    Avalon LW peripheral + /dev/mem + mmap (the canonical pattern we follow)
│   ├── tiles/, ...           VGA tile-graphics reference (Pac-Man-style framebuffer)
│   ├── sample-design-document.pdf   Reference for our own design-doc format
│   ├── wireframe-proposal.pdf       Sample project proposal
│   └── de1-soc-docs/         Terasic board documentation
│       ├── DE1-SoC_User_manual_revf.pdf   Full board manual: pinouts, schematics summary, demos for VGA / WM8731 / SDRAM / HPS
│       ├── learning_roadmap.pdf           Index of Terasic's tutorials (suggested reading order)
│       └── DE1-SoC_FAQ_en.pdf             Common bring-up gotchas
└── <Project>/
    ├── proposal.pdf
    ├── design.pdf            ← read this first; their hw/sw block diagram + register map
    ├── report.pdf            ← post-mortem with what worked / what didn't
    ├── presentation.pdf
    ├── source.tar(.gz|.xz)
    └── source/               Extracted source tree
```

## Subsystem cross-reference

What our design needs vs. which reference to read first.

| Subsystem in our design                | Best reference                  | Why                                                        |
|----------------------------------------|---------------------------------|------------------------------------------------------------|
| Board pinouts, VGA / WM8731 / HPS regs | `_starter/de1-soc-docs/DE1-SoC_User_manual_revf.pdf` | Authoritative source for board signals, demos, and component datasheets referenced from it. |
| Avalon LW slave + `/dev/mem` mmap      | `_starter/lab3-hw`, `lab3-sw`   | Canonical pattern; everything else builds on it.           |
| Two-player turn-based board game       | **Chess**                       | Closest analog — board mem, captures, AI hook (UCI), GUI.  |
| Discrete grid + cursor on VGA          | Tetris, Pac-Man                 | Grid-cell rendering; cursor / piece overlay.               |
| HPS-rendered framebuffer in on-chip BRAM | Chess (`board_mem.sv`)        | BRAM behind an Avalon write port; we address it per-pixel with an 8bpp palette. No reference does a true pixel framebuffer — Pac-Man writes positions, not pixels. |
| VGA tile-mapped graphics (alt path)    | `_starter/tiles`, Poker, Tetris | If we ever want tile-based rendering instead of pixels.    |
| `$readmemh` ROMs (audio/sprites)       | Pac-Man (`*.vh` files), Poker   | `.vh`/`.hex` files loaded at synthesis — same as our audio.|
| WM8731 audio playback via Avalon-ST    | **PianoHeroes** (`project_hw/Audio`), Pac-Man (`audio.vh`) | Direct match for our audio_controller. |
| Audio file → `.vh`/`.hex` toolchain    | Autotune (`PYTHON/wavfile_construction`) | WAV→hex conversion script.                       |
| USB HID keyboard via libusb            | `_starter/lab2`, all video games| HID report parsing, interrupt transfer loop.              |
| Game state machine (menus, modes)      | Tetris, Pac-Man, Poker          | Title/play/game-over screens with shared input.            |
| Software AI / engine integration       | **Chess** (`uci.c`, `uci.h`)    | UCI bridges a chess engine to the GUI — same shape as our MCTS hook. |
| Scoring & game-over logic              | Poker, Tetris                   | Hand evaluation / line-clear scoring.                      |

## Project notes

### Tier 1 — read these first

- **Chess** — `source/hw/{vga_board.sv, board_mem.sv, vga_counters.sv}` and
  `source/sw/{chess.c, uci.c}`. Two-player board game with a grid display, validated moves,
  captures, and an AI engine integration point. The single closest analog to our Go design.
  *Differences from us:* tile-based VGA (theme_mem + board_mem) rather than HPS-rendered
  framebuffer, but the move-validation / capture / AI-engine-handoff structure maps directly.

- **`_starter/lab3-hw` + `lab3-sw`** — `vga_ball.sv` + `vga_ball.c` + `vga_ball.h`. The course's
  reference Avalon LW peripheral, with the matching userspace mmap'd driver. Our `go_peripheral`
  + `hw_iface.c` should follow this skeleton exactly.

- **PianoHeroes** — `source/project_hw/Audio/`, `codec_interface_hw.tcl`, `fpga_intf.sv`. Full
  WM8731 audio path: Qsys glue, audio_controller streaming, sample storage. Use as the
  template for our `audio_controller` and `.vh` ROM loading.

- **Pac-Man** — `source/PacMan/hardware/{audio.vh, *.vh, *.qsys}` and `software/{vga_ball.c,
  controller.c, main.c}`. Audio + sprites stored as `.vh` files loaded with `$readmemh`,
  exactly the pattern we use for stone-place / capture / illegal / game-over sounds. The HPS
  `vga_ball.c` userspace driver is also a close template for `hw_iface.c`.

### Tier 2 — useful for specific patterns

- **Tetris** — `source/CSEE-4840-Project-Tetris/{hw,sw}`. Grid-based VGA game with title /
  play / game-over screens, scoring, and discrete-cell rendering. The `hw/sprite_version (not
  using)` directory is a deliberate dead end — they ended up with tiles.

- **Poker** — `source/{Hardware/, Software/poker_logic.c, Tiles-Parser/}`. Game state machine
  with multiple screens (title, cards, game over via `.hex` ROMs). The Tiles-Parser shows how
  they convert images → hex ROMs — useful template for our audio/sample preprocessing.

- **GeometryDash** — `source/geometry-dash/{hw,sw}` and `hw/gd-tiles`. VGA + audio rhythm
  game; another tile-based reference.

- **FlappyBird** — `source/4840-Flappy-Bird/{lab3-hw, FB_sw}`. Notable because they kept the
  `lab3-hw` skeleton verbatim and built on top — confirms that's a sensible starting point.

### Tier 3 — videogame scaffolding (skim for keyboard/menu/timing)

- **DinoRun** — `source/myfiles/{hardware, software}`. VGA platformer with keyboard input.
- **AirplaneBattle** — `source/upload_spaceship/`. VGA combat game.
- **ForestFireIce** — `source/ForestFireIce/{hw, sw, driver, assets}`. Notable for splitting
  out a `driver/` subdirectory, which we may want to mirror.
- **MMT** — `source/embedded_files/{hw, trackball, game_physics}`. Marble-physics game with a
  trackball — interesting input-handling reference, less relevant for our Go keyboard path.
- **Autotune** — `source/Autotune/{PYTHON, final-hw, final-sw}`. FFT-based audio. The
  `PYTHON/wavfile_construction/` scripts are the direct reference for our WAV-to-`.vh`
  conversion pipeline.

## What's intentionally NOT here

The 2025 cohort also included AccelReg, Barcode, BatMachine, CircuitSim, FPGA-MPC, Gesture,
MNIST, N-Body, PacketFilter, ScreamJump, SonicSecurity, and Systolic. Those are pure hardware
accelerators / non-game / non-CODEC projects with little overlap to our design. Skipped.

Rhythm-Master had no source archive on the course site (only a report) — also skipped.
