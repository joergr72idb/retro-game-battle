# CLAUDE.md - Development kit for the C64 and FujiNet

Toolchain only - real programs live in their own project folders and point `DEV` here.

- Everything is built by `tools/setup.sh` into `opt/` (sources `src/`, logs `build/logs/`); never install tools
  system-wide or into the home folder. FujiNet-PC's Python venv is `build/venv-fujinet`.
- FujiNet-PC is built with `-p IEC` from a pinned master commit: the PC IEC target (NetIEC, PR #2299) is in no
  release yet. Needs mbedTLS 3 via `MBEDTLS_ROOT_DIR`, like the Atari kit.
- VICE is dillera/vice-fn (not stock VICE): only it can hand units 12-30 to the bridge. Always start it with
  `DYLD_LIBRARY_PATH=opt/vice-bridge`, `-config build/vicerc` and `-logfile build/logs/vice-run.log` (vice.log is the build log)
  (otherwise it writes to ~/.config/vice and ~/.local/state/vice).
- fujinet-lib must be >= 5.1.1 for the C64 (reads the per-transaction status byte of current firmware).
- Strings from FujiNet are ASCII - convert to PETSCII before printing (`ascii2pet()` in samples/hello.c).
- Meatloaf's HTTP/TNFS/ML: file systems are not in FujiNet-PC: `OPEN ...,8,...,"http://..."` can't be emulated.
- Verify without watching the screen: start x64sc with `-remotemonitor`, then
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
- Start/stop FujiNet-PC only via `make fujinet` / `make stop`: they match this kit's instance by its full path.
