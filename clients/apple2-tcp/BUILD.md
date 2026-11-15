# Building the Apple II client, step by step

There is no ready-made Apple II disk in `disk-images/`: the boot disk contains ProDOS 2.4.3, and
the [ProDOS 8 releases repository](https://github.com/ProDOS-8/ProDOS8-Releases) states no
licence - so we don't redistribute it. You build `rgbtcp.po` yourself; the kit downloads ProDOS
for you from that repository. It takes one setup run (some minutes) and one `make`.

## What you get

- `build/RGBTCP.SYSTEM` - the client: a ProDOS SYS program (cc65, `-t apple2enh`, load address $2000)
- `rgbtcp.po` - a 140 KB ProDOS boot disk (volume `/RGB`) with `PRODOS` and `RGBTCP.SYSTEM`:
  boot it, and ProDOS starts the first `*.SYSTEM` file by itself

Runs on an Apple //e **enhanced**, //c, //c+ or IIgs (65C02 or better - `apple2enh` code does not
run on an unenhanced //e or a II+) with a FujiNet on the SmartPort. Which Apple II setups the
FujiNet supports: [FujiNet wiki](https://github.com/FujiNetWIFI/fujinet-firmware/wiki).

## 1. Prerequisites (macOS)

```sh
xcode-select --install                 # compiler, make, git
# Homebrew: https://brew.sh
brew install gh
gh auth login                          # the kit downloads the fujinet-lib release with gh
```

Tested on an Apple Silicon Mac with Homebrew (2026-11-14). On Linux (Debian/Ubuntu; the kit and this client tested on Ubuntu 26.04, 2026-11-15) use `tools/setup-linux.sh`
instead, after installing its packages:
`sudo apt install build-essential git cmake ninja-build python3 libsdl3-dev libboost-dev zlib1g-dev libgtk-4-dev libadwaita-1-dev gh unzip curl`
(then `gh auth login`).

## 2. Build the Apple II kit (once)

```sh
cd devkits/development-kit-apple2-and-fujinet       # from the root of this repository
tools/setup-macos.sh                   # Linux: tools/setup-linux.sh
```

`setup-macos.sh` installs `cmake ninja sdl3 boost gh` with Homebrew and builds into the kit's `opt/`
(sources in `src/`, logs in `build/logs/`):

| Part | From | Used for |
|---|---|---|
| cc65 | [cc65/cc65](https://github.com/cc65/cc65) (master) | compiling `rgb_tcp.c` (`cl65`) |
| fujinet-lib 5.1.1 | [FujiNetWIFI/fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) release | the FujiNet network calls (`fujinet-apple2-5.1.1.lib`) |
| cadius 1.4.5 | [mach-kernel/cadius](https://github.com/mach-kernel/cadius) | creating the ProDOS disk image |
| ProDOS 2.4.3 | [ProDOS-8/ProDOS8-Releases](https://github.com/ProDOS-8/ProDOS8-Releases) | `opt/prodos/PRODOS` + `bootblocks.bin` (blocks 0-1 of the release disk) |
| FujiNet Go Apple II 0.10.0 | [FujiNetWIFI/fujinet-go-apple2-desktop](https://github.com/FujiNetWIFI/fujinet-go-apple2-desktop) | the emulator (only for testing) |

Single parts can be rebuilt: `tools/setup-macos.sh cc65 fujinet-lib cadius prodos fngo` (Linux: `tools/setup-linux.sh`, same names).

Check: `opt/cc65/bin/cl65`, `opt/fujinet-lib/fujinet-apple2-*.lib`, `opt/cadius/cadius`,
`opt/prodos/PRODOS` and `opt/prodos/bootblocks.bin` exist.

## 3. Build the client

```sh
cd clients/apple2-tcp                  # from the root of this repository
make SERVER=192.168.17.138             # the default server shown at the start (your server's address)
```

`SERVER` (default `127.0.0.1`), `PORT` (default `6502`) and `NAME` (default `APPLE2`, the player
name sent to the server) are compiled in as defaults. The player can still type another server at
the start, so one disk works with any server. The Makefile finds the kit in
`../../devkits/development-kit-apple2-and-fujinet` (or `../../../development-kit-apple2-and-fujinet`);
`make DEV=/path/to/kit` points it elsewhere.

What `make` does:

1. `cl65 -t apple2enh -O -C apple2enh-system.cfg ... rgb_tcp.c fujinet-apple2-*.lib`
   -> `build/RGBTCP.SYSTEM` (a SYS file that ProDOS loads at $2000).
2. `cadius CREATEVOLUME rgbtcp.po RGB 140KB` - an empty ProDOS volume `/RGB`.
3. `cadius ADDFILE` for `PRODOS` (type SYS, as `PRODOS#FF0000`) and `RGBTCP.SYSTEM` (type SYS, aux
   $2000, as `RGBTCP.SYSTEM#FF2000`) - cadius takes type and aux from the `#` suffix of the name.
4. `dd` the ProDOS boot blocks into blocks 0-1 - cadius doesn't write them, and without them the
   disk doesn't boot.

When `SERVER`/`PORT`/`NAME` change, the Makefile deletes the old binary and disk first, so a
rebuild always has the new default.

Check the disk:

```sh
../../devkits/development-kit-apple2-and-fujinet/opt/cadius/cadius CATALOG rgbtcp.po
#   /RGB/
#     PRODOS          SYS  $0000  ...
#     RGBTCP.SYSTEM   SYS  $2000  ...
```

## 4. Run it

**On a real Apple II with FujiNet:** copy `rgbtcp.po` to the FujiNet's SD card (or a TNFS server
the FujiNet can reach). In FujiNet's CONFIG: pick the host, the file `rgbtcp.po`, drive slot 1,
then boot (ESC). Then:

1. `SERVER: ...` - RETURN keeps the shown server, or type `HOST` or `HOST:PORT`.
2. PIN, or RETURN for none.
3. The game menu (1-4). Joystick or arrow keys steer, button 0 or Space fires (Artillery).
4. After the match: any key plays again, **S** changes the server.

**In the emulator** (Mac, with the game server running on the same Mac, `make SERVER=127.0.0.1`):

```sh
cd devkits/development-kit-apple2-and-fujinet       # from the root of this repository
make run DISK=../../clients/apple2-tcp/rgbtcp.po    # copies the disk onto the emulated SD card
```

In CONFIG: host **SD** -> `rgbtcp.po` -> slot **1** -> **ESC** boots it.

**Already have ProDOS?** `build/RGBTCP.SYSTEM` alone is enough: copy it onto any ProDOS disk as
type SYS ($FF), aux $2000 (e.g. with [CiderPress II](https://github.com/fadden/CiderPress2)) and
start it from there.

## Troubleshooting

- **`gh: not logged in` / fujinet-lib download fails:** `gh auth login`, then
  `tools/setup-macos.sh fujinet-lib` (Linux: `tools/setup-linux.sh fujinet-lib`).
- **ProDOS download fails:** the kit loads it from raw.githubusercontent.com. Don't use
  `releases.prodos8.com` instead - its certificate doesn't match, and switching certificate checks
  off is not a fix.
- **The disk doesn't boot:** boot blocks missing - the last step (`dd`) must have run; rebuild with
  `make clean && make`.
- **"NO FUJINET FOUND."** - `network_init()` found no network device on the SmartPort.
- **"NO CONNECTION TO ..."** - server address/port wrong or the server not running; the client asks
  for the server again on the next round.

Never commit `rgbtcp.po` or `opt/prodos/` - they contain ProDOS (`.gitignore` keeps them out).
