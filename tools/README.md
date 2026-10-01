# Tools

- `cpc_dsk_put.py` - writes a `.bas` source onto a CPC `.DSK` image
  (WinAPE DATA format) as a headerless ASCII file, replacing an existing
  file of the same name - used to refresh `disk-images/cpc/rgb.dsk`
  (see `disk-images/README.md`). Same script as in Retro Tron Battle.
- `atr_dos2_put.py` - adds/replaces/removes files on a single-density
  Atari DOS 2.0S `.ATR` image; `.LST`/`.TXT` targets are converted to
  ATASCII for `ENTER`. Zeroes free sectors and rebuilds the VTOC - used
  for `disk-images/atari/rgb.atr`.
