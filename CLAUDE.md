# CLAUDE.md - Development kit for Atari XL and FujiNet

Toolchain only - real programs live in their own project folders and point `DEV` here (see README).

- Everything is built by `tools/setup-macos.sh` / `tools/setup-linux.sh` (keep their pinned versions in step) into `opt/` (sources `src/`, logs `build/logs/`); never install tools
  system-wide or into the home folder. FujiNet-PC's Python venv is `build/venv-fujinet` (`-V` option of its `build.sh`).
- FujiNet-PC needs mbedTLS **3** (macOS: Homebrew `mbedtls@3` via `MBEDTLS_ROOT_DIR`; Linux: the system `libmbedtls-dev` 3.6); 4.x is not what its CI uses.
- atari800 is built with `--disable-download`: otherwise a missing ROM makes it download PC Xformer ROMs into
  `rom/` next to the config and rewrite `atari800.cfg` - that happened once on 2026-11-14.
- ROM paths in `atari800.cfg` are relative (`rom/...`): start atari800 from the kit folder (the Makefile does).
- Homebrew's `atari800` has no NetSIO - always use `opt/atari800` (7.2.1 from source).
- Start/stop FujiNet-PC only via `make fujinet` / `make stop` (or `tools/run-fujinet`): they match this kit's
  instance by its full path. Never `pkill -x fujinet` - that would also hit other projects' instances.
- Ports: web UI 127.0.0.1:8000, NetSIO UDP 9997 (Makefile `WEB_PORT`/`NETSIO_PORT`; `make fujinet` writes the
  [BOIP] port into fnconfig.ini). The C64 kit uses 8001 + TCP 1541 - both kits run at the same time (tested).
- Restart atari800 only together with FujiNet-PC (`make stop`, then FujiNet-PC first): a new atari800
  never links to a FujiNet-PC that is already running (known from the BBS, hit again 2026-11-14).
- Keyboard joystick: off by default (`-no-kbdjoy0/1`, the BBS needs every key). `make run JOY=1`:
  joystick 0 = cursor keys, fire = Space (`SDL2_JOY_0_*` in atari800.cfg). Mac keyboards have no keypad.
- `opt/fujinet-pc/fnconfig.ini` has CR/LF line endings - sed patterns need `\r`.
- Each emulated power-on makes FujiNet-PC exit 75 (reboot); `tools/run-fujinet` restarts it. That is normal.
- Verify a FujiNet program without watching the screen: `grep 'Fuji cmd' build/logs/fujinet.log` shows the
  commands that reached FujiNet-PC (e.g. `GET ADAPTER CONFIG` from `samples/hello.c`).
- FastBasic loops: `REPEAT ... UNTIL cond` or `DO ... LOOP` (endless, leave with `EXIT`) - no `LOOP UNTIL`.
- ca65 programs: `cl65 -t atari -C atari-asm.cfg`, `.export start` - the cc65 library adds the XEX headers and RUNAD.
