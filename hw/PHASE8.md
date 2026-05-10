# Phase 8 — Title / Menu / Game-Over Polish

**Goal:** the game launches into a title screen with menus instead of jumping straight into PvP. Player navigates the menu with arrow keys, picks PvP or PvC, picks AI level if PvC, plays a game, and on game-over can return to the menu.

**Prerequisites:** Phases 1–7a working. Phase 8 is **purely software** — no HDL changes, no Qsys re-import. It reuses the existing strip framebuffer for menu rendering and uses the existing CURSOR register's `visible` bit to hide the green ring during menus.

## What changed in this commit

- **`sw/go_main.c`** — substantially refactored:
  - New `UiState` enum: `UI_TITLE`, `UI_MODE_SELECT`, `UI_DIFFICULTY`, `UI_GAME`.
  - Three new render helpers: `render_title()`, `render_mode_select(sel)`, `render_difficulty(sel)`. All draw to the same 640×60 strip framebuffer.
  - New state-entry helpers: `enter_title()`, `enter_mode_select()`, `enter_difficulty()`, `enter_game()` — each clears the board and redraws the strip appropriately.
  - Main loop is now a state machine. Key handling dispatches by `ui`.
  - Esc has context-aware behavior: from menu screens it goes back one level; from TITLE / GAME it quits.
  - R from GAME goes back to TITLE (unless the program was launched with the argv shortcut, in which case it just resets the current game).
  - `argv` semantics extended: no arg → start at TITLE; arg `0` → skip menu, PvP; arg `1/2/3` → skip menu, PvC at that level.

## UI flow

```
                        Esc
                  ┌─────────────────┐
                  ▼                 │
   start ──▶ [TITLE]                │
                │ Enter             │
                ▼                   │
            [MODE_SELECT] ──Esc────┘
                │
        ┌───────┴───────┐
   PvP  │       PvC     │
        │               │ Enter
        │               ▼
        │           [DIFFICULTY]
        │            │ Enter         │ Esc
        │            ▼               │
        └────────▶ [GAME] ◀──────────┘
                      │
                      │ R
                      ▼
                  [TITLE]
```

## Strip layouts (visible at the bottom of the VGA monitor)

**TITLE:**
```
                  GO 9X9                    ← scale 4, gold
                                            (centered horizontally)
                PRESS ENTER                 ← scale 2, white
```

**MODE_SELECT:**
```
              SELECT MODE
        PVP                 PVC             ← scale 3
        ↑ green when selected, gray otherwise
```

**DIFFICULTY:**
```
              AI DIFFICULTY
        L1        L2        L3              ← scale 3
        ↑ selected one is green
```

**GAME:** existing live score panel (Phases 5–7).

**GAME_OVER:** existing winner banner (Phase 5).

## Build & run

No HW changes, so:

```bash
cd /path/to/sw
make go_main          # picks up the changes; same Makefile as Phase 7

sudo ./go_main        # launches at TITLE
sudo ./go_main 0      # skip menu, PvP
sudo ./go_main 1|2|3  # skip menu, PvC at level N
```

## Expected on-screen behavior

- After boot + `./go_main`: VGA shows empty board (cleared) with no cursor; strip shows "GO 9×9 / PRESS ENTER" centered.
- Press Enter: strip changes to "SELECT MODE / PVP    PVC" with PVP highlighted green.
- Press Right: PVC turns green, PVP grays out.
- Press Enter on PVC: strip changes to "AI DIFFICULTY / L1 L2 L3" with L2 highlighted (default).
- Press Right twice: L3 highlighted.
- Press Enter: enters game, board cleared, cursor visible at tengen, score panel shows "BLACK: 0 / TURN: BLACK".
- Play normally; on two passes the strip becomes the GAME OVER banner.
- Press R: back to TITLE.
- Press Esc from TITLE: program exits cleanly.

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Title screen shows old game state visible on board | `enter_title()` should call `hw_reset_board()` to clear cells. |
| Menu cursor (the green ring) stays on screen during menus | `enter_*()` helpers must call `hw_cursor(0, 0, 0)` (visible=0). |
| Menu options don't change on left/right | The state machine dispatches by `ui`; check the `case UI_MODE_SELECT:` block actually runs (printf to confirm). |
| Pressing Esc from GAME does something weird | The Esc handler at the top of the loop short-circuits on `UI_GAME` to `goto done` — confirm that path. |
| R during PvC restarts but puts AI as Black | `enter_game()` always starts Black (the human side); PvC always has White as the AI. If you want a "swap colors" option, that's a Phase 8b feature. |

## What's NOT in Phase 8

- No "swap colors" option (AI always plays White in PvC).
- No move history / undo.
- No save/load games.
- No board-size selection (always 9×9 — that's the entire fixed-hardware design).
- No customizable cursor color or board theme.
- Title screen has no animation.

## Project status after Phase 8

| Phase | Status | Tested on board? |
|-------|--------|------------------|
| 0 | lab3 baseline | n/a |
| 1 | static board renderer | not yet |
| 2 | tilemap + stones | not yet |
| 3 | cursor + USB keyboard | not yet |
| 4 | rule engine (PvP) | unit-tested locally; not on board |
| 5 | strip framebuffer | not yet |
| 6 | audio | not yet (placeholder samples) |
| 7a | CPU-only AI | not yet (latency budget at risk) |
| 7b | FPGA rollout accelerator | **NOT IMPLEMENTED** (deferred) |
| 8 | title / menu / game-over polish | not yet |

The next thing to do is **actually build and program the FPGA**, starting from Phase 1 (`hw/PHASE1.md`). Each phase's PHASE*.md file documents the Qsys/toplevel changes required and the expected on-screen behavior. Bring up phase by phase and back off if something breaks — `git checkout phase-N` returns to a known-good state.

If Phase 7a's MCTS is too slow for actual gameplay, Phase 7b (FPGA rollout accelerator per design doc §7.5) is the remaining HDL work to do. That's the only major unbuilt piece.
