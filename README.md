# go_embedded_systems

9×9 Go game on the DE1-SoC (Cyclone V FPGA + ARM HPS).
HW: tilemap board renderer, score-strip framebuffer, audio. SW: game logic, AI, USB keyboard, menu UI.

For first-time bring-up follow [`hw/PHASE_HEAD.md`](hw/PHASE_HEAD.md) end-to-end.
This file is for **iterating** after the project is already working.

## Repo layout

```
hw/         FPGA sources (SystemVerilog), Quartus project, Makefile
hw/codec_interface/  Altera-UP audio core (toplevel, not in Qsys)
sw/         C sources for the ARM, Makefile
references/ unmodified reference projects from prior years (FlappyBird, Pac-Man, etc.)
```

Key HW files:
- `hw/go_peripheral.sv` — top of the IP. VGA timing, board tilemap, cursor, register map.
- `hw/strip_fb.sv` — 640×60 8bpp double-buffered score-strip framebuffer.
- `hw/audio_controller.sv` — 4-sample audio mixer; samples in `place.vh`/`capture.vh`/`illegal.vh`/`gameover.vh`.
- `hw/soc_system_top.sv` — toplevel; instantiates Qsys system + `codec_interface`.
- `hw/soc_system.tcl` — generates the Quartus project (.qpf/.qsf/.sdc) and pin assignments.
- `hw/go_peripheral_hw.tcl` — declares the `go_peripheral` IP to Platform Designer.
- `hw/Makefile` — drives the whole HW build flow.

Key SW files:
- `sw/go_main.c` — full game with title/menu/game-over UI.
- `sw/go_test.c`, `sw/go_phase3.c` — bring-up test programs (board only / cursor only).
- `sw/strip_render.c`+`.h` — score-strip software renderer (clear/text/box helpers).
- `sw/board.c` — Go rules engine.
- `sw/ai.c` — random / greedy / MCTS AI.
- `sw/usbkeyboard.c` — libusb-1.0 boot-protocol HID reader.

## Iterating on hardware

Run all of these from `hw/` on the lab machine (Quartus 21.1 in `$PATH`).

### Editing an IP source file (`go_peripheral.sv`, `strip_fb.sv`, `board_mem.sv`, `audio_controller.sv`, or any `.vh` sample)

```bash
make qsys      # ~30s — re-runs qsys-generate, copies your edited .sv into
               #         soc_system/synthesis/submodules/
make quartus   # ~5–7 min — full re-synthesis + fitter to soc_system.sof
make rbf       # ~10s — converts .sof to .rbf for SD card
```

Then transfer `output_files/soc_system.rbf` to the FAT partition of the lab2 SD card and power-cycle the board. (See "Getting the .rbf onto the SD card" below.)

The `make qsys` step is now (since commit `051222c`) properly tracked as depending on every IP source file via the `IP_SOURCES` var in the Makefile. Without that fix, `make qsys` would say "Nothing to be done" even after you edited an `.sv` file, leaving stale copies in `soc_system/synthesis/submodules/` and silently shipping the wrong bitstream.

### Editing the toplevel (`soc_system_top.sv`)

```bash
make quartus
make rbf
```

No `make qsys` needed — the toplevel sits outside Qsys.

### Editing the codec_interface (`hw/codec_interface/*.v`)

```bash
make quartus
make rbf
```

Same — those files are added directly to the Quartus project via `soc_system.tcl`, not via Qsys.

### Adding a new file to the Qsys IP

1. Add it to `hw/go_peripheral_hw.tcl`'s fileset (an `add_fileset_file ... SYSTEM_VERILOG PATH ...` line).
2. Add the file's name to `IP_SOURCES` in `hw/Makefile` so `make qsys` re-runs when it changes.
3. `make qsys && make quartus && make rbf`.

### Changing pin assignments or project-wide settings

Edit `hw/soc_system.tcl`, then:

```bash
make project-clean    # blow away the generated .qpf/.qsf/.sdc
make project          # quartus_sh + quartus_map + quartus_sta — recreates them
make quartus
make rbf
```

`make project` re-runs `soc_system.tcl`, which is the canonical source for what the .qsf should contain (codec_interface entries, VGA pin assignments, etc.). Never hand-edit the `.qsf` directly — your changes will be lost on the next `make project`.

### Full clean rebuild from scratch

```bash
make project-clean
make qsys-clean
make project          # ~3 min
make quartus          # ~5–7 min
make rbf
```

Use this if you suspect stale state, or after `git pull` brings in changes to `soc_system.tcl` / `go_peripheral_hw.tcl` / the Makefile.

### When `make project` runs, it also...

- Runs `quartus_map` (early synthesis pass) so that
- `quartus_sta -t soc_system/synthesis/submodules/hps_sdram_p0_pin_assignments.tcl` can append HPS DDR3 pin/termination assignments to the `.qsf`. **These are required** — without them, the fitter throws ~1450 OCT termination errors and gives up. If you ever see "Can't fit design in device" with a thousand Error 174068 lines, the HPS pin .tcl didn't run; do `make project-clean && make project`.

## Iterating on software

Run from `sw/` **on the DE1-SoC** (after booting the .rbf):

```bash
make go_main         # ignore any kernel-module errors; we don't use it
sudo ./go_main       # or just ./go_main if you're root@de1-soc
```

CLI shortcuts (skip menus): `./go_main 0` = PvP, `./go_main 1`/`2`/`3` = PvC at AI level 1/2/3.

To ship a SW change to the board, the fastest loop is `scp` (avoid `git pull` on the slow SD card):

```bash
# from your dev machine, after pushing
scp sw/go_main.c sw/strip_render.c root@<board-ip>:/root/go_embedded_systems/sw/
# on board:
make go_main
./go_main
```

`board-ip` is whatever `ip addr show eth0` reports on the DE1-SoC.

## Getting the .rbf onto the SD card

Lab machines may block direct SD card access. The portable workflow:

1. On the lab machine: `git add -f hw/output_files/soc_system.rbf && git commit -m "rbf vN" && git push`. (`-f` because `output_files/` is `.gitignore`d.)
2. On a personal Mac/laptop: `git pull`.
3. Plug SD card in. macOS auto-mounts the FAT partition under `/Volumes/<NAME>/`.
4. **Use Finder** to drag-replace `soc_system.rbf` onto the FAT partition. Terminal `cp` is blocked by macOS TCC unless you grant Full Disk Access.
5. Eject in Finder. Move SD card to DE1-SoC. Power-cycle.

## Common bring-up issues

See `hw/PHASE_HEAD.md` "Common HEAD-state issues" — covers VGA black, scrambled colors, audio dead, MCTS too slow, etc.

One **NOT** covered there (worth knowing):

- **Strip framebuffer shows duplicated/shifted text in lower rows.** Means the strip pixel-address path is too narrow. Should be 16 bits everywhere from `strip_pixel_full` (in `go_peripheral.sv`) through `pixel_addr` (in `strip_fb.sv`); 14 bits truncates and wraps every 25 strip rows. Fixed in commit `45c999e` if you're on a fresh clone; if you reverted, check those widths.
