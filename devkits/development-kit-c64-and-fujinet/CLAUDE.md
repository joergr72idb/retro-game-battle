# CLAUDE.md - Development kit for the C64 and FujiNet

Toolchain only - real programs live in their own project folders and point `DEV` here.

- Everything is built by `tools/setup-macos.sh` / `tools/setup-linux.sh` (keep their pinned versions in step) into
  `opt/` (sources `src/`, logs `build/logs/`); never install tools system-wide or into the home folder. FujiNet-PC's Python venv is `build/venv-fujinet`.
- FujiNet-PC is built with `-p IEC` from a pinned master commit: the PC IEC target (NetIEC, PR #2299) is in no
  release yet. Needs mbedTLS 3 (macOS: via `MBEDTLS_ROOT_DIR`, like the Atari kit;
  Linux: the system `libmbedtls-dev`).
- VICE is dillera/vice-fn (not stock VICE): only it can hand units 12-30 to the bridge. Always start it with
  `DYLD_LIBRARY_PATH=opt/vice-bridge` (Linux: `LD_LIBRARY_PATH`), `-config build/vicerc` and `-logfile build/logs/vice-run.log` (vice.log is the build log)
  (otherwise it writes to ~/.config/vice and ~/.local/state/vice).
- fujinet-lib must be >= 5.1.1 for the C64 (reads the per-transaction status byte of current firmware).
- Strings from FujiNet are ASCII - convert to PETSCII before printing (`ascii2pet()` in samples/hello.c).
- Meatloaf's HTTP/TNFS/ML: file systems are not in FujiNet-PC: `OPEN ...,8,...,"http://..."` can't be emulated.
- Check a program headless: `make shot [PRG=] [FRAMES=] [KEYS=]`, then read `build/shot.txt` / `build/shot.png`
  (tools/c64shot.py: x64sc with SDL dummy video/sound and `-binarymonitor` on MON_PORT 6520; keyboard feed 0x72,
  keymatrix 0x74, joystick 0xa3 - VICE holds them N frames itself; display 0x84 + palette 0x91; text screen found via
  $D018/$DD00). Timing is wall clock at 50 fps (VICE runs at real speed). Tested 2026-11-15: hello-c, text, F1, cursor,
  matrix key, both joysticks + fire. cc65 programs switch to the lower case charset - the text dump follows $D018.
- By hand: start x64sc with `-remotemonitor`, then
  `printf 'm 0400 07e7\n' | nc -w 2 127.0.0.1 6510 > build/logs/screen.txt; tools/screen.py build/logs/screen.txt`.
  `grep -a 'Fuji cmd' build/logs/fujinet.log` lists commands that reached FujiNet-PC (`-a`: the log can contain NULs).
- Keys from outside: the monitor's `keybuf` drops control characters (cursor keys etc.) - write them
  into the key buffer instead: `> 0277 91` + `> 00c6 01` (91 = CRSR up, 11 down, 9d left, 1d right).
  FujiNet-PC logs `NetworkProtocolTCP::write(n)` for every network write of a C64 program.
- Never truncate `build/logs/fujinet.log` while FujiNet-PC runs (leaves NUL bytes) - `make stop` first, then `rm`.
- Keyboard: VICE runs with `keymaps/c64-mac-de.vkm` (German Mac). The C64 "<-" key ($5F) is FujiNet CONFIG's
  ESC (leave CONFIG = boot) - VICE's own maps put it on ` (dead key on German Macs), so it was unreachable.
  VICE keymap files have CRLF line endings: search them with `LC_ALL=C` and strip `\r`.
- Ports: web UI 127.0.0.1:8001, NetIEC TCP 1541 (Makefile `WEB_PORT`/`NETIEC_PORT`; `make fujinet` writes the
  [BOIP] port into fnconfig.ini, the bridge gets `NETIEC_PORT`). The Atari kit uses 8000 + UDP 9997 - both kits
  run at the same time (tested).
- `make fujinet` runs `tools/check-ports` first (refuses when another program listens on WEB_PORT or
  NETIEC_PORT - FujiNet-PC is the TCP server, VICE's bridge the client); `make run`/`config` also refuse when
  something is already connected to NETIEC_PORT (a second VICE). Like the Atari kit's check (2026-11-15).
  Tested: `make run NETIEC_PORT=1542 WEB_PORT=8003` works; afterwards fnconfig.ini keeps that port until the
  next `make fujinet`.
- Start/stop FujiNet-PC only via `make fujinet` / `make stop`: they match this kit's instance by its full path.
- macOS SIP strips `DYLD_*` when a program starts via `/usr/bin/python3` (or `/usr/bin/env`, `/bin/sh`): `make shot`
  hands the bridge path to tools/c64shot.py as `BRIDGE_LIBPATH`/`BRIDGE_DIR`, which sets it for VICE. Symptom of a
  missing bridge: "Real Device: Real device access is not available!" in build/shot.vice.log, "no fujinet found".
