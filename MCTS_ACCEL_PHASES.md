# MCTS Accelerator — Build, Test, and Troubleshoot on the Linux Dev Machine

This is the **operational** companion to `design-document.md` (§7.5) and the
implementation plan in `.claude/plans/snug-splashing-hare.md`. It tells you
how to actually run the toolchain: every command, its expected output, the
failures you will hit, and how to fix them.

Audience: someone sitting at the DE1-SoC development Linux box with Quartus
21.1, ModelSim-Altera, the SoC EDS toolchain, and a DE1-SoC board on the
desk.

---

## 0. Prerequisites — verify your toolchain

Run each command and check the version line. Expected versions in
parentheses; older may work but is untested by these procedures.

```
$ quartus_sh --version           # 21.1.0 Lite (matches references)
$ qsys-script --version          # 21.1
$ qsys-generate --version        # 21.1
$ vsim -version                  # ModelSim 10.5b SE-64 or later
$ which arm-linux-gnueabihf-gcc  # the Linaro cross-compiler
$ python3 --version              # 3.8+
$ dtc --version                  # 1.6+
$ git --version
```

If any are missing, that's the first blocker. The Columbia 4840 lab machines
have all of them on `$PATH` after `source /opt/Intel-FPGA/quartus/21.1/setup.sh`
(or your local equivalent).

Confirm the SoC EDS environment is sourced:
```
$ echo $SOCEDS_DEST_ROOT
/opt/Intel-FPGA/embedded
$ which bsp-create-settings
/opt/Intel-FPGA/embedded/host_tools/altera/preloader/bsp-create-settings
```

If `SOCEDS_DEST_ROOT` is empty, source the setup script:
```
$ source $SOCEDS_DEST_ROOT/env.sh
```
(on Columbia machines: `source /opt/Intel-FPGA/embedded/env.sh`).

---

## 1. Where everything lives

```
.
├── Makefile                      ← top-level orchestrator (make help)
├── hw/
│   ├── Makefile                  ← Quartus + Qsys
│   ├── soc_system.qsys           ← edited via qsys-edit or add_mcts_accel.tcl
│   ├── soc_system_top.sv         ← pins
│   ├── add_mcts_accel.tcl        ← one-shot Qsys integration script
│   ├── ip/mcts_accel/            ← the new IP block
│   └── output_files/             ← .sof, .rbf, fitter reports (after compile)
├── sw/
│   ├── Makefile
│   ├── board.c, ai.c             ← SW reference + tree MCTS + dispatch
│   ├── ai_mcts_hw.c, hw_iface.c  ← HPS userspace driver for mcts_accel
│   ├── go_self_test.c            ← bring-up smoke tool
│   ├── go_main.c                 ← the full game (Phase 9: heat + Tab + Y)
│   └── golden/                   ← Python reference + vector generator
├── sim/
│   ├── Makefile                  ← make sim_flood_fill / sim_rollout / sim_mcts
│   ├── *_tb.sv                   ← testbenches
│   ├── run_*.do                  ← ModelSim scripts
│   └── vectors/                  ← regenerable .txt files
└── scripts/
    ├── build_sd_image.sh
    └── flash_sd.sh
```

---

## 2. Phase A — Verify SW baseline (before touching any HW)

The SW path is the floor: AI Level 3 plays in pure C on the Cortex-A9 with
no FPGA involvement. Everything else is upside. **Confirm this works
first.** If it doesn't, no amount of FPGA work will fix it.

### A.1 Host unit tests (any machine — your laptop is fine)
```
$ cd sw
$ make board_test && ./board_test
```
Expected: `All tests passed.` ~30 board-rule cases.

### A.2 Host self-test (no /dev/mem needed)
```
$ make go_self_test
$ ./go_self_test selftest
open /dev/mem: No such file or directory     ← expected on host
self_test: OK
```
The first line is OK — `ai_init()` correctly falls back to SW when there's
no FPGA. The second line means the SW dispatch path is wired correctly.

### A.3 Smoke-test compile of `go_main`
```
$ make go_main
```
This pulls in board + ai + strip + audio + USB + hw_timer + ai_mcts_hw +
hw_iface. **Common failure**: missing `-lusb-1.0`. Install
`libusb-1.0-0-dev` (apt) or equivalent.

If it builds cleanly, the SW side is ready.

---

## 3. Phase B — Simulate the new RTL

This is where you spend the most iteration time. The novel module
(`flood_fill.sv`) WILL have bugs in its first revision; the testbench
framework is structured to surface them fast.

### B.1 Generate vectors
```
$ cd sim
$ make vectors
wrote .../sim/vectors/ff_vectors.txt
wrote .../sim/vectors/re_vectors.txt
wrote .../sim/vectors/mc_vectors.txt
```
The Python golden model (`sw/golden/go_model.py`) is the source of truth.
**If you change the algorithm in `flood_fill.sv`, mirror the change in
`go_model.py` first and regenerate vectors.** Otherwise the DUT is being
graded against a stale reference.

### B.2 Simulate `flood_fill.sv` (the novel module)
```
$ make sim_flood_fill
```

Expected on first run: **the DUT will fail some vectors.** That's why this
target exists. You'll see something like:
```
line 3: expected 000000000000000000201 02 1
         got      000000000000000000001 01 0
FAIL: 4 mismatches in 7 lines
```
Workflow when this fails:
1. Open `sim/flood_fill_dut.txt` and `sim/vectors/ff_vectors.txt` side by
   side. The expected file has comments naming the test case.
2. Re-run in interactive ModelSim to see waveforms:
   ```
   $ vsim -do run_flood_fill.do
   ```
   In the GUI, find the first failing vector and step through. Common bugs:
   - Wrong `shift_w` / `shift_e` mask direction → wave shows the wavefront
     spilling into the wrong column.
   - `done` asserted too early → frontier had cells left but we stopped.
   - `has_liberty` set when it shouldn't be → check the `lib_hits` AND with
     `~propagate_mask` (capturing a liberty inside the group is a bug).
3. Fix in `hw/ip/mcts_accel/flood_fill.sv`, re-run `make sim_flood_fill`.

### B.3 Simulate `rollout_engine.sv`
```
$ make sim_rollout
```
Single-rollout-per-vector mode (SIMS_PER_ENGINE=1). The current `re_vectors.txt`
has a few hand-crafted positions. Once `flood_fill.sv` is clean, most
failures here are FSM logic bugs in `rollout_engine.sv`:
- Forgetting to restore `black`/`white` snapshots on suicide-revert
- Wrong `ko_b`/`ko_w` rotation in APPLY
- Score classifier mixing up `seen_black_border` vs `seen_white_border`

Add a hand-crafted "easy capture" vector to `gen_vectors.py` and re-run.

### B.4 Simulate `mcts_accel.sv` (top level)
```
$ make sim_mcts
```
NUM_ENGINES=1, full Avalon-MM protocol. This vector mostly checks:
- Streaming 162 board-load writes lands in the right bits
- START → DONE round-trip completes
- 82 result reads come back consistent

Failures here are typically register decode (`avs_address` case statement
in `mcts_accel.sv`) or outer-FSM state-transition bugs.

### B.5 When all three sims are green
You have a fully-verified accelerator at NUM_ENGINES=1, ready to go to
Quartus.

---

## 4. Phase C — Quartus synthesis

### C.1 First-time: add mcts_accel to Qsys

The base `soc_system.qsys` is the lab3 starter (HPS only). You need to add
`mcts_accel` plus whatever go_peripheral / strip_fb / audio modules your
larger project uses. For the MCTS piece specifically:

```
$ cd hw
$ qsys-script --script=add_mcts_accel.tcl --quartus-project=soc_system
mcts_accel_0 added to soc_system.qsys
```

This adds the component instance, wires clock/reset/bus, and sets the base
address to `0x0020`. You can also do it via the GUI:
```
$ qsys-edit soc_system.qsys
```
In the IP catalog (left pane), search "MCTS_ACCEL" — it should appear under
"csee4840 > mcts". Drag it into the system; connect clock/reset/bus by hand;
set base address to `0x0020`.

**Common failure**: `qsys-script` says "module mcts_accel not found in IP
catalog". Fix: the IP search path needs `hw/ip/`. The script tries
`set_project_property IP_SEARCH_PATHS "ip/**/*"` which works when run from
`hw/`. If you're elsewhere, use:
```
$ qsys-script --script=add_mcts_accel.tcl \
              --search-path=hw/ip/mcts_accel,$  \
              --quartus-project=hw/soc_system
```

### C.2 Regenerate Qsys
```
$ qsys-generate soc_system.qsys --synthesis=VERILOG
```
~30 seconds. Produces `soc_system/synthesis/soc_system.qip` and Verilog
under `soc_system/synthesis/submodules/`.

**Common failure**: `Avalon-MM slave 'mcts_accel_0.avalon_slave_0' has
overlapping address range`. Fix: check that `go_peripheral` doesn't extend
past `0x1F`. If it does, move `mcts_accel`'s base address higher.

### C.3 Full Quartus compile, fast variant (NUM_ENGINES=1)
```
$ make hw FAST=1
```
~5–10 minutes on a typical 4840 machine. Expected output ending with:
```
Quartus Prime Compilation Complete.
```

The fitter report is at `output_files/soc_system.fit.summary`:
```
$ cat output_files/soc_system.fit.summary
...
Logic utilization (in ALMs)     : N,NNN / 32,070 (n %)
Total registers                 : N,NNN
Total block memory bits         : NN,NNN / 4,065,280 (n %)
Total DSP Blocks                : N / 87 (n %)
```
At NUM_ENGINES=1 expect: ~3,500 ALMs, ~3 M10K blocks. If you see something
2× that, run `quartus_sta` and look at the resource breakdown by hierarchy
to find what's blowing up.

### C.4 Convert to .rbf for SD card boot
```
$ make rbf
```
Produces `output_files/soc_system.rbf`.

---

## 5. Phase D — Boot Linux on the DE1-SoC

### D.1 Build the SD card image
The `scripts/build_sd_image.sh` script is a skeleton — fill in step 6
(image assembly) for your local SoC EDS install on first use. Typical flow:

```
$ ./scripts/build_sd_image.sh
== 1. Regenerate Qsys ==
== 2. Quartus full compile ==        (skipped if .sof is current)
== 3. Convert .sof to .rbf ==
== 4. Preloader (SPL) ==
== 5. Device tree ==
== 6. Assemble image ==
== Done: hw/output_files/sdcard.img ==
```

If you don't have a base image yet, follow the Columbia 4840 instructions
to build one from `socfpga-4.19` once; this script then refreshes it on
each iteration.

### D.2 Flash + boot
```
$ lsblk                  # find your SD card; e.g. /dev/sdc
$ SD_DEV=/dev/sdc ./scripts/flash_sd.sh
About to dd .../sdcard.img → /dev/sdc
Type 'yes' to continue: yes
...
$ # eject, reseat into DE1-SoC, power-cycle
```

Connect over UART (`screen /dev/ttyUSB0 115200` or `minicom`). U-Boot
should load the preloader, then the kernel. Log in as root.

### D.3 Verify the new device-tree node
```
# ls /proc/device-tree/sopc@0/bridge@0xc0000000/
mcts_accel_0
go_peripheral_0
hps_0_bridges
...
# cat /proc/device-tree/sopc@0/bridge@0xc0000000/mcts_accel_0/reg | od -An -tx1
 00 00 00 20 00 00 00 ff
```
The `reg` value should encode `0xFF200020 (base) + 0x000000ff (length-1)`.
If the node is missing, the device tree wasn't regenerated — go back to
step D.1 step 5.

---

## 6. Phase E — Bring-up tests on the board

### E.1 Probe the accelerator
Copy your built binaries to the board (over scp via Ethernet) and:
```
# ./go_self_test probe
probe: OK
```
**`probe: hw_init_mcts() failed`** means one of:
1. `/dev/mem` permission denied → run as root (`sudo` or login as root)
2. `mmap` failed → the LW bridge base or span is wrong; check
   `LW_BRIDGE_BASE = 0xFF200000` and `LW_BRIDGE_SPAN = 0x00200000` in
   `sw/hw_iface.h`
3. Accelerator's `running` bit is stuck high → reset path didn't propagate;
   check that the `add_mcts_accel.tcl` reset connection (`clk_0.clk_reset`)
   matches the connection actually present in the (possibly hand-edited)
   `soc_system.qsys`.

### E.2 FSM smoke test
```
# ./go_self_test selftest
self_test: OK
```
This runs 200 sims via the accelerator and verifies:
- Total visits == 200 (FSM ran the right number of rollouts)
- At least 5 distinct cells were visited (FSM isn't wedged on one cell)
- Total wins is between 5% and 95% (FSM isn't always reporting win/loss)

**`visits=N != MCTS_SIMS=200`** is the most common failure here:
- N=0 → the engine never finished; DONE signal didn't propagate. Check
  `eng_done[gi]` aggregation and the outer FSM `all_done` wire.
- N=1 → one rollout completed, then the engine wedged. Re-run with
  SignalTap on FSM `state` to find where it loops.
- N=200 but `distinct_cells < 5` → the engine completes rollouts but
  always reports the same `first_cell`. Bug in PICK_RANDOM (LFSR not
  advancing? mod-popcount returning 0?).

### E.3 SW vs HW agreement
```
# ./go_self_test ab 1000
ab: leaves=1000 samples=NNNN MAE=0.04NN
```
PASS criterion: `MAE < 0.10`. With 25 sims per leaf × 1 engine, expected
per-cell standard error is ~0.10; the MAE should be well under that.

**`MAE > 0.10`** with otherwise-clean self_test usually means a *bias* in
the HW rollout — not a crash, just wrong play. Common causes:
- Capture detection misses a corner case (eye shape, snake group along edge)
- Suicide check is too aggressive (rejecting legal moves) or too lax
- Scoring touches_b / touches_w classification is wrong-direction

Drop in a SignalTap capture on a single rollout and print the move
sequence to compare against the SW reference.

### E.4 Full self-play game
```
# ./go_main 3       # PvC, AI Level 3 = MCTS
```
Black is human; white is AI. Each AI turn should print:
```
AI (White) plays (4,3).  [HW, 1.42 ms]
```
The `HW` is from `ai_backend_label()`. If it says `SW`, the dispatcher
never flipped to HW path — probably `hw_init_mcts()` returned -1, see E.1.

Press **Tab** during the game to flip backend → next AI move says `SW`
with a much longer ms. Press **Y** after an AI move to replay the same
board with the other backend (live A/B for the demo).

---

## 7. Phase F — Scale to NUM_ENGINES=8

```
$ make hw            # drops the FAST=1 override; NUM_ENGINES=8
```
~15–25 minutes. **Expected first-attempt failure**: timing closure at
100 MHz. The fitter summary will say:
```
Timing Analyzer Status                            : Failed
Worst-Case Slack (slow 1100mV 85C model)          : -1.234 ns
```

Open the TimeQuest report:
```
$ quartus_sta soc_system -c soc_system
```
Look at the "Critical Paths" table. The path will almost always involve:
- `popcount81` (81-bit adder tree)
- The `nth_set_bit` priority encoder (81-deep chain)
- DRAIN-state summation (loop over NUM_ENGINES)

**Fix path 1**: register the `popcount` output. Edit
`rollout_engine.sv`, change `assign legal_count = popcount81(legal_mask)`
to a registered version with one extra cycle of latency before the
PICK_RANDOM transition. Re-synth.

**Fix path 2**: if path 1 isn't enough, register the priority encoder too
(`pick_bit` becomes registered). +1 cycle in PICK_RANDOM.

**Fix path 3**: register the DRAIN summation. In `mcts_accel.sv`, the
inner loop summing `eng_wins[g][drain_i]` for 8 engines can grow long;
pipeline it over 2-3 cycles by accumulating across multiple drain
iterations.

**If all three pipeline passes still fail to close at 100 MHz**: drop to
NUM_ENGINES=4:
```
$ make hw NUM_ENGINES=4
```
Or drop the clock to 50 MHz (edit `soc_system.sdc` — set
`create_clock -period 20.000` on `CLOCK_50` instead of an inferred faster
clock). At 4 engines × 50 MHz you still get ~250× speedup over SW.

After timing closes, rebuild SD image (`make sd_image`) and reboot. Re-run
all of Phase E. The `selftest` and `ab` numbers should be similar (HW
correctness shouldn't change with engine count); the wall-clock should be
roughly 8× faster (`go_main 3` reports the per-turn ms).

---

## 8. Troubleshooting — common failure modes

### 8.1 `quartus_map: invalid SystemVerilog construct` in `flood_fill.sv`
Quartus 21.1's SV support is incomplete. If `function automatic`
declarations or `unique case` cause errors, replace with classical
`always_comb` blocks or explicit `if / else if` chains. The
`mcts_accel_pkg::popcount81` function is the most likely target.

### 8.2 Address 0xFF200020 reads as 0xFFFFFFFF on the board
The slave isn't being decoded. Three checks:
1. `qsys-generate` actually ran after `add_mcts_accel.tcl`. Look for
   `mcts_accel_0` in `soc_system/synthesis/submodules/`.
2. The base address in Qsys really is `0x0020` (not `0x20` interpreted as
   words = `0x80` bytes, or some other off-by-something).
3. The bitstream you booted matches the device tree. If you rebuilt the
   .rbf but not the .dtb, the kernel may not know the slave is there.

### 8.3 `./go_main` segfaults immediately on the board
Both `hw_init` (from `go_main.c` for go_peripheral) and `hw_iface.c::hw_init`
(for MCTS) `mmap` /dev/mem. On the DE1-SoC, /dev/mem requires root. Run as
root or wrap with sudo. The two mmaps coexist (different fds, same physical
region) — this is intentional, not a bug.

### 8.4 Heat overlay flashes briefly, then disappears
That's correct behavior for the HW path. Tree MCTS publishes heat every
8 sims over ~1 s of compute → 25 visible heat snapshots. The HW path runs
in ~1.4 ms → 1 snapshot, single frame visible. To make HW heat sticky for
demo purposes, hold `hw_overlay_enable(1)` longer in `go_main.c`
`apply_ai_move` (e.g., `usleep(200000)` after the AI call).

### 8.5 ModelSim: `# ** Error: (vsim-PLI-3001) ... Failed to load`
Stale `work/` library. From `sim/`:
```
$ make clean
$ make sim_flood_fill
```

### 8.6 `make sim_flood_fill` passes but `make sim_rollout` fails on
"unable to read vectors/re_vectors.txt"
You skipped vector generation. Run `make vectors` first, or let the
`*_vectors.txt: ...` Makefile dependency do it for you (it should — if it
doesn't, your `make` is broken; try `gmake` explicitly).

### 8.7 SignalTap II capture shows the FSM stuck in `RUN`
Engine never asserts `done`. The most common cause is an inner FSM that
took an unexpected transition and is now spinning in `PICK_RANDOM` with
`legal_count == 0` repeatedly:
- Check `consecutive_passes` is actually incrementing in `TRY_PASS`
- Check the `MOVE_CAP` exit (`move_count >= MOVE_CAP`) triggers
- Look for an `unique case` that's hitting a `default` and silently
  reverting state

### 8.8 The HPS hangs after writing `MCTS_START`
The Avalon slave never asserted readdata. Check:
- The `readdata` always-block in `mcts_accel.sv` covers all addresses (it
  has a `default: avs_readdata <= 32'h0`)
- The slave `readWaitTime` / `readLatency` in `mcts_accel_hw.tcl` matches
  the actual SV timing. We have `readLatency=1, readWaitTime=0` which
  means "readdata is valid one cycle after read goes high". If the SV
  always-block latches readdata combinationally instead, change to
  `readLatency=0`.

### 8.9 `make hw` fails: "couldn't find file mcts_accel_pkg.sv"
The Qsys component fileset doesn't reference it. Check
`hw/ip/mcts_accel/mcts_accel_hw.tcl` lists ALL `.sv` files in the
`add_fileset_file` lines. We have 5; if you add more (e.g.,
`tt_score.sv`), append them.

### 8.10 The HW path returns all-zero results even though probe passes
Most likely: outer FSM exits IDLE on START write, but engines never see
`broadcast_start` because of a one-cycle pulse vs registered signal mismatch.
In `mcts_accel.sv`, the BROADCAST state should pulse `broadcast_start <= 1`
on entry; in `rollout_engine.sv`, the IDLE state should latch the inputs
when `start` is high. If `broadcast_start` is only high for the BROADCAST
cycle and engines aren't ready by then, they miss it. Fix by extending
`broadcast_start` for an extra cycle, or by having the engine sample
inputs in IDLE continuously while waiting.

---

## 9. Decision rules — when to abandon and fall back

These are data-driven, not time-driven (per the plan file). Fire whenever
the trigger fires:

| Trigger | Action |
|---|---|
| `flood_fill.sv` can't pass all `ff_vectors.txt` after >3 rounds of debug | Reduce algorithmic scope: skip seki detection (count seki cells as neutral territory). Document the limitation in the report. |
| NUM_ENGINES=8 fails timing closure after 3 pipeline-register passes | Drop to NUM_ENGINES=4 at 100 MHz. |
| NUM_ENGINES=4 still fails timing at 100 MHz | Drop clock to 50 MHz at NUM_ENGINES=4. Still ~250× speedup. |
| `selftest` passes but `ab` MAE > 0.15 over 10,000 leaves | Treat as a known bias; document it. SW path remains for production. |
| Anything at all is broken at demo time | Press **Tab** in `go_main` to flip to SW backend. Game continues at Level 3 (~1–2 s per AI turn). Recover and continue. |

The SW path is **always** the demo-safety net. The `ai_toggle_backend()`
function makes the failover a single keystroke.

---

## 10. Cheat sheet — all commands in one place

### Build
```
make help                   # list all targets
make sw                     # cross-compile userspace
make hw FAST=1              # Quartus build, NUM_ENGINES=1 (~5–10 min)
make hw                     # Quartus build, NUM_ENGINES=8 (~15–25 min)
make hw NUM_ENGINES=4       # explicit override
make sd_image               # rebuild SD image (after make hw)
SD_DEV=/dev/sdX make flash_sd  # write to SD card
```

### Simulate
```
make -C sim vectors         # regenerate golden vectors from go_model.py
make -C sim sim_flood_fill  # ModelSim headless, diff vs golden
make -C sim sim_rollout
make -C sim sim_mcts
vsim -do sim/run_flood_fill.do   # interactive with waveforms
```

### On the board
```
./go_self_test probe        # hw_init_mcts() succeeded?
./go_self_test selftest     # FSM not wedged?
./go_self_test ab 1000      # 1000-leaf SW vs HW agreement
./go_main                   # full game, menu-driven
./go_main 3                 # skip menus, PvC at Level 3
# In-game: Tab = toggle SW/HW backend, Y = replay with other backend
```

### Inspect resources
```
cat hw/output_files/soc_system.fit.summary
quartus_sta soc_system -c soc_system     # TimeQuest details
```

### Iterate fast
```
make hw FAST=1 && make sd_image && SD_DEV=/dev/sdc make flash_sd
# eject, reseat, ssh in, ./go_self_test ab 1000
```

---

## 11. What to put in your report

Once the system works end-to-end:

1. **Fitter resource numbers** from `output_files/soc_system.fit.summary`
   at NUM_ENGINES=8 (or your final config).
2. **Achieved Fmax** from `output_files/soc_system.sta.summary`.
3. **Per-AI-turn wall-time**: `./go_main` prints the time for each AI
   move. Capture 100 samples for SW and HW; report mean ± stddev and
   speedup ratio.
4. **A/B agreement**: `./go_self_test ab 10000`. Report MAE; this is
   your "the HW path is functionally correct" evidence.
5. **SignalTap waveform** showing one rollout in progress — `state`
   transitions, `eng_done`, `cycles_count`. Captured on the actual board.
6. **One block diagram** from §3 of the design doc; one inner detail
   diagram (e.g., the flood_fill wavefront).

Total report figures: ~6. The narrative around them is the §7.5 design
walkthrough with implementation experience layered on top.

---

## 12. Compatibility notes (Phase 9 heat + backend toggle)

The accelerator and the heat-overlay / SW-HW-toggle Phase 9 features are
designed to coexist cleanly. Key points to remember during integration:

- **Register ranges don't overlap.** `go_peripheral` occupies bytes
  `0x00..0x16` (board + cursor + heat + timer regs). `mcts_accel` lives at
  bytes `0x20..0x40` in the same LW bridge. Anything you add to
  `go_peripheral` must stay below `0x20`.
- **Two `/dev/mem` mmaps coexist.** `go_main.c` has its own
  `static int hw_init(void)` (for go_peripheral byte-write access) and
  `hw_iface.c` has a public `int hw_init(void)` (for MCTS 32-bit access).
  Both map the same physical region into different virtual addresses.
  This is intentional. Don't try to merge them unless you've checked
  every call site.
- **The Tab key toggles `g_use_hw_mcts`, not the eval function pointer.**
  - HW probed OK → Tab flips between tree-MCTS (SW) and HW flat-MCTS.
  - HW probe failed → Tab flips between tree-MCTS (SW) and *flat-MCTS on
    CPU* via the dispatcher. Both run on the Cortex-A9; the timer shows
    the difference in algorithmic depth, not HW-vs-SW.
  - `ai_backend_label()` returns "HW" in the second case (mechanically
    correct — the dispatcher is the "HW path"); the comment in `ai.h`
    documents this behavior.
- **The heat callback fires from both paths now.** Tree-MCTS publishes
  every 8 sims (live progress). HW flat-MCTS publishes once at the end
  (single snapshot — visible for a single frame). If you want the HW
  snapshot to be sticky for demo viewing, add `usleep(200000)` between
  the AI call and `hw_overlay_enable(0)` in `apply_ai_move`.
- **Timer registers (0x10..0x16) are in go_peripheral, NOT mcts_accel.**
  The MCTS_STATUS register's `cycles_count` field is a *separate*
  free-running counter inside mcts_accel that times only the rollout
  computation (BROADCAST to DRAIN). Don't conflate.

---

## 13. One-page glossary

| Term | Meaning |
|---|---|
| LW bridge | HPS-to-FPGA lightweight Avalon-MM master at `0xFF200000` on HPS, span 2 MB. |
| ALM | Adaptive Logic Module — Cyclone V's logic primitive. ≈ 2.66 LEs. |
| M10K | 10-Kbit on-chip block RAM. |
| flood_fill | Wavefront BFS over an 81-bit bitboard. Used for capture/suicide/scoring. |
| rollout_engine | One independent playout-to-terminal MCTS rollout machine. |
| genvar | SystemVerilog `generate`-loop iterator. Used to fan out N engines. |
| Tromp-Taylor | Chinese-style area scoring rule. Each empty region bordering one color counts for that color. |
| komi | White's compensation for moving second. We use 5.5 → integer 5 + white wins ties. |
| LFSR | Linear-feedback shift register; cheap pseudo-random source. Ours is 16-bit Fibonacci with taps {16,15,13,4}. |
| .rbf | Raw bitstream file, loaded by U-Boot from the SD card's VFAT partition. |
| SignalTap II | Quartus's on-chip logic analyzer. Embeds in the bitstream; captures over JTAG. |
