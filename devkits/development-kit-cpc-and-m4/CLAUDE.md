# CLAUDE.md - Development kit for the CPC and the M4 board

Toolchain only - real programs live in their own project folders and point `DEV` here.

- Everything is built by `tools/setup-macos.sh` / `tools/setup-linux.sh` (keep their pinned versions in step) into
  `opt/` (sources `src/`, logs `build/logs/`). SDCC is the Homebrew one (macOS) or the apt one (Linux).
  CPCSyntaxError is pinned (`CPCSE_TAG`): it changes almost daily - update on purpose and retest.
- The emulator stores its settings next to the program unless `CPCSE_HOME` is set: always run it with
  `CPCSE_HOME=build/cpcse-home` (the Makefile does).
- Headless `--shot` ignores `--roms` and looks for `roms/` in the current folder: run it from `opt/cpcse`
  with absolute paths (`make shot`). Check results by reading `build/shot.png` (macOS converts with `sips`,
  Linux with `tools/bmp2png.py`). Typing (`--type`) needs ~600 frames before the result is on screen;
  fewer and the text never appears.
- Don't put `\` before `&` in `CMD` - it reaches the CPC literally.
- `RUN"X"` of a binary resets the firmware, and the CPC restarts when the program returns: samples wait
  for a key. `LOAD` of a binary into &4000 needs `MEMORY &3FFF` first (else "Memory full").
- SDCC: `crt0.s` must be the first object (its JP at &4000 is the entry); `--code-loc 0x4010`.
  `tools/cpcbin.py` turns the .ihx into an AMSDOS binary. iDSK takes one `-i` per call.
- M4 protocol details verified against Duke's examples and the emulator source: sizes and ports are
  little-endian, IP addresses last octet first, the socket table updates by itself (poll it).
  `|HTTPGET`/`|HTTPMEM` are not emulated by the "1984" emulator - CPCSyntaxError does them.
- Test server: `make www` (127.0.0.1:8765, samples/www). `build/logs/www.log` shows the requests that
  arrived - a quick way to verify network code without the screen.
