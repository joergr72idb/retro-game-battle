# Building the Atari C client, step by step

A ready-made build is in `disk-images/atari/rgbtcp.xex` (default server 192.168.17.138, any other
can be typed at the start). Build it yourself to change the default server or the code.

## What you get

- `rgbtcp.xex` - the client: an Atari binary (cc65, `-t atari`) that talks to the FujiNet directly
  over SIO (fujinet-lib). No DOS and no `N:` handler needed: the FujiNet boots the XEX itself.

Runs on an Atari XL/XE with a FujiNet. Built with `-t atari`, not `-t atarixl`: fujinet-lib calls
the OS SIO vector, which the atarixl runtime switches out (the call would hang).

## 1. Prerequisites (macOS)

```sh
xcode-select --install                 # compiler, make, git
# Homebrew: https://brew.sh
brew install gh
gh auth login                          # the kit downloads the fujinet-lib release with gh
```

Tested on an Apple Silicon Mac with Homebrew (2026-11-14). On Linux (Debian/Ubuntu; the kit tested on Ubuntu 26.04, 2026-11-15) use `tools/setup-linux.sh`
instead, after installing its packages:
`sudo apt install build-essential git autoconf automake cmake pkg-config libsdl2-dev libmbedtls-dev python3-venv gh unzip curl`
(then `gh auth login`).

## 2. Build the Atari kit (once)

```sh
cd devkits/development-kit-atari-xl-and-fujinet     # from the root of this repository
tools/setup-macos.sh                   # Linux: tools/setup-linux.sh
```

`setup-macos.sh` installs `autoconf automake cmake pkgconf sdl2 mbedtls@3 gh` with Homebrew and builds
into the kit's `opt/` (sources in `src/`, logs in `build/logs/`):

| Part | From | Used for |
|---|---|---|
| cc65 | [cc65/cc65](https://github.com/cc65/cc65) (master) | compiling `../common/rgb_core.c` and `platform.c` (`cl65`) |
| fujinet-lib 5.1.0 | [FujiNetWIFI/fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) release | the FujiNet network calls (`fujinet-atari-5.1.0.lib`) |
| atari800 7.2.1 | [atari800/atari800](https://github.com/atari800/atari800) | the emulator, with NetSIO (only for testing; Homebrew's atari800 has no NetSIO) |
| FujiNet-PC 1.6.1 | [FujiNetWIFI/fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware) | the FujiNet as a Mac program (only for testing) |
| FastBasic 4.8.1 | [dmsc/fastbasic](https://github.com/dmsc/fastbasic) | not needed for this client |

Single parts can be rebuilt: `tools/setup-macos.sh cc65 fujinet-lib atari800 fastbasic fujinet-pc` (Linux: `tools/setup-linux.sh`, same names).

Check: `opt/cc65/bin/cl65` and `opt/fujinet-lib/fujinet-atari-*.lib` exist (for building);
`opt/atari800/bin/atari800` and `opt/fujinet-pc/fujinet` (for testing).

**ROMs (emulator only):** the original Atari OS/BASIC ROMs are copyrighted and not included. Put your
own into the kit's `rom/` (`atari800.cfg` expects `REV02.ROM` = 800XL OS rev. 2, see the kit's
README); without them atari800 uses its built-in, free AltirraOS.

## 3. Build the client

```sh
cd clients/atari-tcp                   # from the root of this repository
make SERVER=192.168.17.138             # the default server shown at the start (your server's address)
```

`SERVER` (default `127.0.0.1`), `PORT` (default `6502`) and `NAME` (default `ATARIXL`, the player
name sent to the server) are compiled in as defaults. The player can still type another server at
the start. The Makefile finds the kit in `../../devkits/development-kit-atari-xl-and-fujinet` (or
`../../../development-kit-atari-xl-and-fujinet`); `make DEV=/path/to/kit` points it elsewhere.

What `make` does: `cl65 -t atari -O ... ../common/rgb_core.c platform.c fujinet-atari-*.lib` -> `rgbtcp.xex` (and a
linker map in `build/`). When `SERVER`/`PORT`/`NAME` change, the Makefile deletes the old binary
first, so a rebuild always has the new default.

## 4. Run it

**On a real Atari with FujiNet:** copy `rgbtcp.xex` to the FujiNet's SD card (or a TNFS server the
FujiNet can reach). In FujiNet's CONFIG: pick the host, the file `rgbtcp.xex`, drive slot 1, boot -
the FujiNet loads XEX files itself. Then:

1. `SERVER: ...` - RETURN keeps the shown server, or type `HOST` or `HOST:PORT`.
2. PIN, or RETURN for none.
3. The game menu (1-4). Joystick 1 or CTRL+arrow keys steer, button or Space fires (Artillery).
4. After the match: any key plays again, **S** changes the server.

**In the emulator** (Mac, with the game server running on the same Mac, `make SERVER=127.0.0.1`):

```sh
make run          # FujiNet-PC boots rgbtcp.xex in atari800; the Mac's cursor keys + Space = joystick 1
make config       # afterwards: FujiNet-PC back to its CONFIG menu at boot
```

`make run` copies `rgbtcp.xex` onto the kit's FujiNet SD card, mounts it in slot 1 with CONFIG off
(`fn_boot.py` edits `fnconfig.ini`), restarts FujiNet-PC and then starts atari800 - FujiNet-PC
always first. F10 (on a Mac maybe fn+F10) saves a screenshot as `atari000.pcx` in the kit folder.

## Troubleshooting

- **`gh: not logged in` / fujinet-lib download fails:** `gh auth login`, then
  `tools/setup-macos.sh fujinet-lib` (Linux: `tools/setup-linux.sh fujinet-lib`).
- **Keys do nothing at the prompts (emulator):** another disk booted first - e.g. a DOS + `N:`
  handler disk in D1: that the program overwrites. Use `make run` (it mounts the XEX as the only
  boot disk), not `atari800 -run`.
- **Buzzing / joystick dead during a match:** a build older than 3. The client switches SIO's noisy
  I/O off (`SOUNDR`) and reads the joystick from the hardware (`PORTA`/`TRIG0`), because the OS
  doesn't update `STICK0`/`STRIG0` while SIO is busy - which is nearly always here.
- **"NO CONNECTION TO ..."** - server address/port wrong or the server not running; the client asks
  for the server again on the next round.
- **atari800 without FujiNet:** atari800 must be started after FujiNet-PC and with `-netsio 9997`
  (`make run` does both).
- **Self test or a black screen:** another FujiNet-PC + atari800 on the same ports (9997/8000, e.g. a
  Blues BBS instance on the same computer) mixes with the kit's: one FujiNet joins the other's Atari.
  With the kit's `tools/check-ports`, `make run` stops with the reason instead. Stop the other
  instance completely (including a restart loop around its FujiNet-PC), or use other ports:
  `make run NETSIO_PORT=9996 WEB_PORT=8002`.
