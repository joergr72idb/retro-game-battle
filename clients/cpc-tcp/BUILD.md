# Building the CPC client, step by step

A ready-made build is in `disk-images/cpc/rgb.dsk` (default server 192.168.17.138, any other can be
typed at the start). Build it yourself to change the default server or the code.

## What you get

- `RGBTCP.BIN` - the client (SDCC, Z80): a binary with an AMSDOS header, loaded and started at &4000.
  It talks to the M4 board's TCP sockets through the kit's own C library `lib/m4`.
- `rgbtcp.dsk` - a disc image with `RGBTCP.BIN` (made with iDSK)

Runs on a Schneider/Amstrad CPC with an M4 board (tested as a CPC 6128 in the emulator). The server
can be an IP address or a host name - the M4 looks names up.

## 1. Prerequisites (macOS)

```sh
xcode-select --install                 # compiler, make, git
# Homebrew: https://brew.sh
```

Tested on an Apple Silicon Mac with Homebrew (2026-11-14). On Linux (Debian/Ubuntu; the kit tested on Ubuntu 26.04, 2026-11-15) use `tools/setup-linux.sh`
instead, after installing its packages:
`sudo apt install build-essential git cmake ninja-build libsdl2-dev libgl-dev sdcc`.

## 2. Build the CPC kit (once)

```sh
cd devkits/development-kit-cpc-and-m4              # from the root of this repository
tools/setup-macos.sh                   # Linux: tools/setup-linux.sh
```

`setup-macos.sh` installs `cmake ninja sdl2 sdcc` with Homebrew and builds into the kit's `opt/` (sources
in `src/`, logs in `build/logs/`):

| Part | From | Used for |
|---|---|---|
| SDCC 4.6.0 | [Homebrew](https://formulae.brew.sh/formula/sdcc) ([SDCC](https://sdcc.sourceforge.net/)) | compiling C for the Z80 (`sdcc`), assembling `crt0.s` (`sdasz80`) |
| iDSK 0.20 | [cpcsdk/idsk](https://github.com/cpcsdk/idsk) | creating the `.dsk` disc image |
| RASM 3.3.3 | [EdouardBERGE/rasm](https://github.com/EdouardBERGE/rasm) | Z80 assembler - not needed for this client |
| CPCSyntaxError 0.16.0 | [WacKEDmaN/CPCSyntaxError](https://github.com/WacKEDmaN/CPCSyntaxError) | the emulator with M4 emulation (only for testing); brings the CPC firmware and Duke's M4 ROM |

Part of the kit itself (in git, not downloaded): `lib/cpc` (SDCC startup code `crt0.s`, `printf` and
keyboard through the CPC firmware), `lib/m4` (the M4 library: sockets, DNS, HTTP - written from
Duke's documented command interface, [m4info.txt](http://www.spinpoint.org/cpc/m4info.txt)) and
`tools/cpcbin.py` (`.ihx` -> binary with AMSDOS header).

Single parts can be rebuilt: `tools/setup-macos.sh rasm idsk cpcse` (Linux: `tools/setup-linux.sh`, same names).

Check: `brew --prefix sdcc` works, `opt/idsk/iDSK` exists (for building); `opt/cpcse/cpcse` (for
testing).

## 3. Build the client

```sh
cd clients/cpc-tcp                     # from the root of this repository
make SERVER=192.168.17.138             # the default server shown at the start (your server's address)
```

`SERVER` (default `127.0.0.1`, an IP address or a host name), `PORT` (default `6502`) and `NAME`
(default `SCHNEIDERCPC`, the player name sent to the server) are compiled in as defaults. The player
can still type another server at the start. The Makefile finds the kit in
`../../devkits/development-kit-cpc-and-m4` (or `../../../development-kit-cpc-and-m4`);
`make DEV=/path/to/kit` points it elsewhere.

What `make` does:

1. `sdasz80` assembles the kit's `lib/cpc/crt0.s`; `sdcc -mz80 --opt-code-size` compiles
   `rgb_tcp.c`, `lib/cpc/cpc.c` and `lib/m4/m4.c` into `build/*.rel`.
2. `sdcc --no-std-crt0 --code-loc 0x4010 --data-loc 0` links them, crt0 first (its jump at &4000 is
   the entry), into `build/rgb_tcp.ihx`.
3. `tools/cpcbin.py` turns that into `RGBTCP.BIN` with an AMSDOS header (load and run at &4000).
4. `iDSK rgbtcp.dsk -n`, then `iDSK rgbtcp.dsk -i RGBTCP.BIN` -> `rgbtcp.dsk`.

When `SERVER`/`PORT`/`NAME` change, the Makefile deletes the old object, binary and disc first, so a
rebuild always has the new default.

## 4. Run it

**On a real CPC with M4:** copy `RGBTCP.BIN` to the M4's SD card (or use `rgbtcp.dsk` wherever you use
disc images). On the CPC, with the M4's SD card as the current drive: `RUN"RGBTCP"`. Then:

1. `SERVER: ...` - ENTER keeps the shown server, or type `HOST` or `HOST:PORT`.
2. PIN, or ENTER for none.
3. The game menu (1-4). Joystick or cursor keys steer, fire button or Space fires (Artillery).
4. After the match: any key plays again, **S** changes the server.

**In the emulator** (Mac, with the game server running on the same Mac, `make SERVER=127.0.0.1`):

```sh
make sd                                            # copy RGBTCP.BIN onto the emulated M4 SD card (m4sd/)
cd ../../devkits/development-kit-cpc-and-m4 && make run    # CPC 6128 + M4, then: RUN"RGBTCP"
```

Headless, with a screenshot: `make shot CMD='RUN"RGBTCP"'` in the kit (`build/shot.png`) - the
emulator stands still when its window is hidden, `shot` doesn't need one.

## Troubleshooting

- **The emulator runs an old version:** it runs the copy on its SD card - `make sd` after every build.
- **"UNKNOWN HOST ..."** - the M4 couldn't look the name up; check the spelling or use the IP address.
- **"NO CONNECTION TO ..."** - server address/port wrong or the server not running; the client asks
  for the server again on the next round.
- **"NO M4 BOARD FOUND."** - the M4 ROM isn't present (emulator: M4 switched off; the kit's
  `make run` writes `m4=1` into the emulator's settings).
- **`sdcc` not found:** `brew install sdcc` (on macOS the Makefile takes it from `brew --prefix sdcc`);
  Linux: `sudo apt install sdcc` (taken from PATH).
- **Timing:** the emulated M4 answers about ten times faster than real hardware - check steering on a
  real CPC + M4.
