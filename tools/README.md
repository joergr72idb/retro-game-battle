# Tools

- `cpc_dsk_put.py` - writes a `.bas` source onto a CPC `.DSK` image
  (WinAPE DATA format) as a headerless ASCII file, replacing an existing
  file of the same name - used to refresh `disk-images/cpc/rgb.dsk`
  (see `disk-images/README.md`). Same script as in Retro Tron Battle.
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
