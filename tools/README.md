# Tools

- `cpc_dsk_put.py` - writes a `.bas` source onto a CPC `.DSK` image
  (WinAPE DATA format) as a headerless ASCII file, replacing an existing
  file of the same name. Was used for the CPC BASIC client; today
  `disk-images/cpc/rgb.dsk` is built by `clients/cpc-tcp/`'s Makefile (iDSK).
  Same script as in Retro Tron Battle.
- `atr_dos2_put.py` - adds/replaces/removes files on a single-density
  Atari DOS 2.0S `.ATR` image; `.LST`/`.TXT` targets are converted to
  ATASCII for `ENTER`. Zeroes free sectors and rebuilds the VTOC - used
  for `disk-images/atari/rgb.atr`.
- `game_screenshots.py` - the game screenshots in `docs/screenshots/games/` (README, "The games"):
  runs the real game modules and the real display with an invisible window (SDL dummy driver) and
  two scripted players that can see the board, and saves one frame per game. Run from the repo root
  with the project's Python: `.venv/bin/python tools/game_screenshots.py [OUTDIR]` (default
  `docs/screenshots/games`). Writes no statistics or log. Rerun it when the display changes; a new
  game needs its own scripted players in the script.
- `handsfree_test.sh` - plays one match per C client with nobody at the keyboard:
  `tools/handsfree_test.sh [atari] [c64] [cpc] [apple2]` (default all four). Starts the server
  headless on 127.0.0.1:6602 (`RGB_PORT`) with its own statistics and log in `build/handsfree/` (the
  event's `game_results.csv` stays untouched), builds each client for it, starts a bot as the
  opponent and runs the client in its development kit's `make shot` with a key script (RETURN,
  RETURN, 1 = Tron, joystick moves). Prints PASS/FAIL per client from the server log (connected,
  match started and ended, moves arrived); exit status 0 = all passed. Screenshots and logs in
  `build/handsfree/`. The Atari runs on NetSIO 9996/web 8002, so it works next to another
  FujiNet-PC + atari800 on the default ports. Afterwards the clients in `clients/*-tcp/` are built
  for the test server - run `make` there again before using them.
