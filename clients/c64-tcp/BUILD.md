# Building the C64 client, step by step

A ready-made build is in `disk-images/c64/rgb.d64` (default server 192.168.17.138, any other can be
typed at the start). Build it yourself to change the default server or the code.

## What you get

- `rgbtcp.prg` - the client (cc65, `-t c64`), talking to the FujiNet's network device 16 over the
  serial bus (fujinet-lib)
- `rgbtcp.d64` - a disk image with the program as `RGBTCP` (made with VICE's `c1541`)

Runs on a C64 with a FujiNet on the IEC bus - for example the LOLIN D32 Pro "Meatloaf" board flashed
with FujiNet's `fujinet-iec-lolin-d32` firmware. A Meatloaf firmware is not enough: it has no FujiNet
network device.

## 1. Prerequisites (macOS)

```sh
xcode-select --install                 # compiler, make, git
# Homebrew: https://brew.sh
brew install gh
gh auth login                          # the kit downloads the fujinet-lib release with gh
```

Tested on an Apple Silicon Mac with Homebrew (2026-11-14). The kit's scripts are written for macOS;
on Linux the same tools build, but `tools/setup.sh` would need adapting (Homebrew, `sysctl`).

## 2. Build the C64 kit (once)

```sh
cd devkits/development-kit-c64-and-fujinet         # from the root of this repository
tools/setup.sh
```

`setup.sh` installs `autoconf automake cmake pkgconf sdl2 sdl2_image mbedtls@3 gh xa dos2unix bison`
with Homebrew and builds into the kit's `opt/` (sources in `src/`, logs in `build/logs/`):

| Part | From | Used for |
|---|---|---|
| cc65 | [cc65/cc65](https://github.com/cc65/cc65) (master) | compiling `rgb_tcp.c` (`cl65`) |
| fujinet-lib 5.1.1 | [FujiNetWIFI/fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) release | the FujiNet network calls (`fujinet-c64-5.1.1.lib`) - 5.1.1 or newer: older releases misread every reply from current firmware |
| VICE (FujiNet fork) | [dillera/vice-fn](https://github.com/dillera/vice-fn), commit `7f4e552` | `c1541` for the disk image; the emulator and its NetIEC bridge (`libopencbm.dylib`) for testing |
| FujiNet-PC (IEC) | [FujiNetWIFI/fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware), commit `8cc29e8b5` | the FujiNet as a Mac program on the emulated serial bus (only for testing; NetIEC is on master only) |

Single parts can be rebuilt: `tools/setup.sh cc65 fujinet-lib fujinet-pc vice`.

Check: `opt/cc65/bin/cl65`, `opt/fujinet-lib/fujinet-c64-*.lib` and `opt/vice/bin/c1541` exist (for
building); `opt/vice/bin/x64sc`, `opt/vice-bridge/libopencbm.dylib` and `opt/fujinet-pc/fujinet`
(for testing). VICE brings its own C64 ROMs.

## 3. Build the client

```sh
cd clients/c64-tcp                     # from the root of this repository
make SERVER=192.168.17.138             # the default server shown at the start (your server's address)
```

`SERVER` (default `127.0.0.1`), `PORT` (default `6502`) and `NAME` (default `C64FUJINET`, the player
name sent to the server) are compiled in as defaults. The player can still type another server at
the start. The Makefile finds the kit in `../../devkits/development-kit-c64-and-fujinet` (or
`../../../development-kit-c64-and-fujinet`); `make DEV=/path/to/kit` points it elsewhere.

What `make` does:

1. `cl65 -t c64 -O ... rgb_tcp.c fujinet-c64-*.lib` -> `rgbtcp.prg` (and a linker map in `build/`).
2. `c1541 -format "rgb tcp,01" d64 rgbtcp.d64 -write rgbtcp.prg rgbtcp` -> `rgbtcp.d64`.

When `SERVER`/`PORT`/`NAME` change, the Makefile deletes the old program and disk first, so a
rebuild always has the new default.

## 4. Run it

**On a real C64 with FujiNet:** copy `rgbtcp.d64` to the FujiNet's SD card (or a TNFS server the
FujiNet can reach). In FujiNet's CONFIG: pick the host, the file `rgbtcp.d64` and a drive slot, then
leave CONFIG with the **←** key, which boots the mounted disk. If the client doesn't start by itself:
`LOAD"RGBTCP",8` and `RUN` (with the disk on device 8). Then:

1. `SERVER: ...` - RETURN keeps the shown server, or type `HOST` or `HOST:PORT`.
2. PIN, or RETURN for none.
3. The game menu (1-4). Joystick in **port 2** or the cursor keys steer, fire button or Space fires
   (Artillery).
4. After the match: any key plays again, **S** changes the server.

**In the emulator** (Mac, with the game server running on the same Mac, `make SERVER=127.0.0.1`):

```sh
make run          # the kit's FujiNet-PC + VICE, autostarts rgbtcp.prg
```

On a German Mac keyboard the C64 **←** key is on **§** or **ß** (the kit's keymap).

## Troubleshooting

- **`gh: not logged in` / fujinet-lib download fails:** `gh auth login`, then
  `tools/setup.sh fujinet-lib`.
- **"NO CONNECTION TO ..." in VICE every time:** VICE must load the FujiNet bridge -
  `DYLD_LIBRARY_PATH=opt/vice-bridge` and `NETIEC_PORT=1541`, which the kit's `make run` sets. Started
  by hand without them, VICE has no FujiNet. Also: FujiNet-PC must be running (`make fujinet` in the kit).
- **Don't use VICE's warp mode:** the serial bus timeouts run in emulated time, FujiNet-PC answers in
  real time - in warp mode every connection fails.
- **Garbled upper/lower case:** cc65 turns string literals into PETSCII; the client converts every
  line to ASCII on the way out and back to PETSCII on the way in. Keep that when changing the code.
- **"NO CONNECTION TO ..." on real hardware:** server address/port wrong or the server not running;
  the client asks for the server again on the next round.
