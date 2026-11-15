# CLAUDE.md - Development kit for the CPC and the M4 board

Toolchain only - real programs live in their own project folders and point `DEV` here.

- Everything is built by `tools/setup-macos.sh` / `tools/setup-linux.sh` (keep their pinned versions in step) into
  `opt/` (sources `src/`, logs `build/logs/`). SDCC is the Homebrew one (macOS) or the apt one (Linux).
  CPCSyntaxError is pinned (`CPCSE_TAG`): it changes almost daily - update on purpose and retest.
- The emulator stores its settings next to the program unless `CPCSE_HOME` is set: always run it with
  `CPCSE_HOME=build/cpcse-home` (the Makefile does).
- `make shot` (since 2026-11-16) = real time: `CPCSE_HIDDEN_WINDOW=1` + `--api=API_PORT` (6128), driven by
  tools/cpcshot.py (JSON lines: `type` with wait=1, `key` = 4 frames down/4 up, `frames N`, `screenshot`, `quit`).
  Cursor keys/Ctrl reach the program as joystick 0 (joystick emulation on in cpcse.ini). Tested with HELLOC, a
  restart + `PRINT 6*7`, and RGB's CPC client vs server + bot (moves arrived). The old headless `--shot` run is
  `make fastshot`: it runs unthrottled (~2x real time), which broke a network match timed in frames.
- Never run `cpcse` without `CPCSE_HOME` (it writes settings next to the program); `cpcse --help` is no option -
  it starts the GUI. Options are in src/gui/main.cpp, the API in docs/external-debugging.md.
- Headless `--shot` (`make fastshot`) ignores `--roms` and looks for `roms/` in the current folder: run it from
  `opt/cpcse` with absolute paths. Check results by reading `build/shot.png` (macOS converts with `sips`,
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
- Optional fujinet-network-cpc (GPL-3, our own repo): setup part `fujinet` clones it at `FNCPC_REV` into
  `src/fujinet-network-cpc` (not in the default run - the kit stays MIT); `make fujinet` builds
  samples/fnhello.c -> FNHELLO.BIN. Test: `make shot CMD='RUN"FNHELLO"' FRAMES=600`, then `make stop`
  (two GET /hello.txt in www.log). Bump `FNCPC_REV` in both setup scripts together.
