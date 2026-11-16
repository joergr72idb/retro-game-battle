# How to: the development kits

How to build the four development kits the clients of this repo are built and tested with,
and how to run each client in its emulator against a local server. Tested on an Apple Silicon Mac
with Homebrew, 2026-11-14. The steps below are the macOS ones; on Linux (Debian/Ubuntu, tested on
Ubuntu 26.04, 2026-11-15) each kit has `tools/setup-linux.sh` instead, and its README lists the apt
packages to install first (SDCC is then the apt one, 4.5.0).
On Windows the kits should run in **WSL2** (Ubuntu 26.04) with the Linux steps - not tested yet;
each kit's README has a section "Windows (WSL2)" (clone into `~/`, not `/mnt/c`; WSLg for the
windows; mirrored networking for connections from real computers into WSL, e.g. to the game server).

| Kit | Toolchain | Emulator + network | Client |
|---|---|---|---|
| `development-kit-atari-xl-and-fujinet` | cc65, fujinet-lib 5.1.0, FastBasic | atari800 7.2.1 (NetSIO) + FujiNet-PC | `clients/atari-tcp/` (C), `clients/atari/` (Atari BASIC) |
| `development-kit-c64-and-fujinet` | cc65, fujinet-lib 5.1.1 | VICE (dillera/vice-fn) + NetIEC bridge + FujiNet-PC (IEC) | `clients/c64-tcp/` |
| `development-kit-cpc-and-m4` | SDCC, RASM, iDSK, `lib/m4` | CPCSyntaxError 0.16.0 with M4 emulation | `clients/cpc-tcp/` |
| `development-kit-apple2-and-fujinet` | cc65, fujinet-lib 5.1.1, cadius, ProDOS 2.4.3 | FujiNet Go Apple II 0.10.0 | `clients/apple2-tcp/` |

The kits are part of this repo, in `devkits/`. They contain only our own files - build scripts,
Makefiles, emulator configs, a keymap, the CPC libraries `lib/m4` and `lib/cpc`, samples. No
upstream sources and nothing copyrighted: every tool is cloned from its own repository and built
on your machine (the full list with links is in the README, "Thanks"). The upstream projects are
used unmodified, at the versions pinned in each kit's setup scripts.

```
retro-game-battle/
├── clients/                 the game clients
└── devkits/
    ├── development-kit-atari-xl-and-fujinet/
    ├── development-kit-c64-and-fujinet/
    ├── development-kit-cpc-and-m4/
    └── development-kit-apple2-and-fujinet/
```

The client Makefiles use `devkits/<kit>` once its setup script has run (its `opt/` exists),
otherwise a checkout of the kit next to this repo (`../development-kit-...`); `make DEV=/path/to/kit`
overrides both. The kits are also kept as separate repositories and merged in with
`git subtree --squash`; to update one:
`git subtree pull --prefix=devkits/<kit> <kit repository> main --squash`.

Every kit has the same shape: `tools/setup-macos.sh` / `tools/setup-linux.sh` clone the upstream sources into `src/` and builds
everything into `opt/` inside the kit (nothing system-wide except Homebrew or apt packages), build logs go
to `build/logs/`, versions are pinned at the top of both scripts, and a `Makefile` has `run`/`stop`
targets. Emulator settings are kept in `build/`, never in the home folder. `src/`, `opt/` and
`build/` are not in git. The kit's own README has the details; this page is the overview and the
build steps.

**Not included, because copyrighted - bring your own:**
- the original Atari OS/BASIC ROMs (into `devkits/development-kit-atari-xl-and-fujinet/rom/`,
  ignored by git); without them atari800 uses the free AltirraOS
- ProDOS 2.4.3 states no licence, so it is downloaded by the setup script and never committed - nor is
  `clients/apple2-tcp/rgbtcp.po`, which contains it
- the CPC firmware and Duke's M4 ROM come with CPCSyntaxError (Amstrad's firmware is free for
  emulators, the M4 ROM is included there with Duke's permission) and stay in the kit's `opt/`

## 0. Prerequisites

Every command block starts in the root folder of this repo.

Each C client also has its own step-by-step `BUILD.md` (in `clients/<client>/`), with checks and
troubleshooting for that platform.

```sh
xcode-select --install                     # compilers, make, git
# Homebrew: https://brew.sh
brew install gh python@3.13
gh auth login                              # the setup scripts download fujinet-lib releases with gh
```

For the game server (needs Python 3.10+, the macOS system Python 3.9 is too old):

```sh
"$(brew --prefix python@3.13)/bin/python3.13" -m venv .venv
.venv/bin/pip install pygame
.venv/bin/python server/game_battle_server.py 127.0.0.1 6502 8080 --headless   # or without --headless
```

## Ports

All kits and the server can run at the same time:

| What | Port |
|---|---|
| Game server | TCP 6502 (raw protocol), 8080 (HTTP bridge) |
| Atari kit: FujiNet-PC | web UI 127.0.0.1:8000, NetSIO UDP 9997 |
| C64 kit: FujiNet-PC | web UI 127.0.0.1:8001, NetIEC TCP 1541 |
| CPC kit | test web server 127.0.0.1:8765 |
| Apple II kit: FujiNet Go | 1985 (SLIP), web UI 64001 |

The FujiNet web UIs have no login and listen on 127.0.0.1 only.

## 1. Atari XL + FujiNet

**Build:**

```sh
cd devkits/development-kit-atari-xl-and-fujinet
tools/setup-macos.sh             # Linux: tools/setup-linux.sh; or single parts: cc65 fujinet-lib atari800 fastbasic fujinet-pc
make                             # samples -> build/*.xex
```

What `setup-macos.sh` does, step by step (`setup-linux.sh` the same, with apt packages):

| Part | Source | Build |
|---|---|---|
| Homebrew | - | `brew install autoconf automake cmake pkgconf sdl2 mbedtls@3 gh` |
| cc65 | `cc65/cc65` master | `make && make install PREFIX=opt/cc65` |
| fujinet-lib | release `v5.1.0` | `gh release download v5.1.0 -R FujiNetWIFI/fujinet-lib -p "*atari*"`, unzip into `opt/fujinet-lib` |
| atari800 | `atari800/atari800` tag `ATARI800_7_2_1` | `./autogen.sh && ./configure --prefix=opt/atari800 --target=default --with-video=sdl2 --with-sound=sdl2 --disable-download && make install` |
| FastBasic | `dmsc/fastbasic` tag `v4.8.1` | `git submodule update --init && make`, copy `build/compiler/` |
| FujiNet-PC | `FujiNetWIFI/fujinet-firmware` tag `v1.6.1` | `git submodule update --init --recursive`, then `MBEDTLS_ROOT_DIR=$(brew --prefix mbedtls@3) ./build.sh -c -y -p ATARI -V build/venv-fujinet`, copy `build/dist` to `opt/fujinet-pc` |

**ROMs:** put the original Atari ROMs (`REV02.ROM` = 800XL OS, BASIC `REVC.ROM`, ...) into the
kit's `rom/` - they are copyrighted and not in git; `atari800.cfg` points there. Without them
atari800 uses its built-in AltirraOS.

**Run the C client:**

```sh
cd clients/atari-tcp
make SERVER=127.0.0.1            # rgbtcp.xex, 127.0.0.1 as the default server
make run                         # the FujiNet-PC boots rgbtcp.xex in atari800 (joystick = cursor keys + Space)
make config                      # FujiNet-PC back to its CONFIG menu (e.g. for the BASIC client's disk)
```

**Run the BASIC client:**

1. Copy `disk-images/atari/rgb.atr` (from this repo) into `opt/fujinet-pc/SD/` of the kit.
2. In the kit folder, start FujiNet-PC, then atari800 with BASIC (joystick 0 = cursor keys, fire = Space):
   ```sh
   make fujinet
   opt/atari800/bin/atari800 -config atari800.cfg -kbdjoy0 -no-kbdjoy1 -xl -basic -netsio 9997
   ```
   (`make run JOY=1 XEX=...` does the same for an `.xex` program.)
3. FujiNet CONFIG boots: pick host **SD**, the image `rgb.atr`, slot 1, boot. Or mount it in the
   web UI (127.0.0.1:8000) beforehand.
4. In BASIC: `ENTER"D:RGB.LST"`. The server address is line 40 of the listing
   (`192.168.17.138` on the ready-made disk) - for a local server type
   `40 HOST$="127.0.0.1"`, then `RUN`.

Pitfalls:
- Homebrew's atari800 has no NetSIO - it must be the 7.x build from source.
- Build atari800 with `--disable-download`, otherwise it fetches ROMs and rewrites its config.
- C with fujinet-lib: `-t atari`, not `-t atarixl` (the SIO call hangs).
- Restart atari800 and FujiNet-PC together, FujiNet-PC first (`make stop`, `make run`).
- atari800 is built without libpng: F10 screenshots only work with a `.pcx` name
  (`-screenshots shot###.pcx`); on a Mac keyboard F10 may need **fn**.

## 2. C64 + FujiNet

**Build:**

```sh
cd devkits/development-kit-c64-and-fujinet
tools/setup-macos.sh             # Linux: tools/setup-linux.sh; or single parts: cc65 fujinet-lib fujinet-pc vice
make
```

| Part | Source | Build |
|---|---|---|
| Homebrew | - | `brew install autoconf automake cmake pkgconf sdl2 sdl2_image mbedtls@3 gh xa dos2unix bison` |
| cc65 | `cc65/cc65` master | as for the Atari |
| fujinet-lib | release `v5.1.1` (older releases misread current firmware replies) | `gh release download v5.1.1 -R FujiNetWIFI/fujinet-lib -p "fujinet-lib-c64-*.zip"` |
| FujiNet-PC (IEC) | `FujiNetWIFI/fujinet-firmware` commit `8cc29e8b5` (NetIEC is on master only) | `./build.sh -c -y -p IEC -V build/venv-fujinet` with `MBEDTLS_ROOT_DIR` as above |
| VICE + bridge | `dillera/vice-fn` commit `7f4e552` | in `vice/`: `./autogen.sh`, out-of-tree `configure --prefix=opt/vice --enable-sdl2ui --disable-pdf-docs --disable-html-docs YACC="$(brew --prefix bison)/bin/bison -y"`, `make install`; then `make -C fujinet/opencbm-netiec` and copy `libopencbm.dylib` + `netiecctl` to `opt/vice-bridge` |

The bridge (`libopencbm.dylib`) is what VICE loads for "real devices": it sends the emulated
serial bus over TCP 1541 to FujiNet-PC (devices 16 network, 29 clock, 30 FujiNet).

**Run the client:**

```sh
cd clients/c64-tcp
make SERVER=127.0.0.1            # rgbtcp.prg + rgbtcp.d64, 127.0.0.1 as the default server
make run                         # FujiNet-PC + VICE, autostarts the client
```

Pitfalls:
- German Mac keyboard: the kit's `keymaps/c64-mac-de.vkm` puts the C64 **←** key on § / ß / \`
  (needed to leave FujiNet CONFIG).
- cc65 turns string literals into PETSCII: convert to ASCII before sending and back on receive.
- Don't use VICE warp mode with FujiNet: the IEC timeouts run in emulated time, FujiNet answers
  in real time ("NO CONNECTION").

## 3. Amstrad/Schneider CPC + M4

**Build:**

```sh
cd devkits/development-kit-cpc-and-m4
tools/setup-macos.sh             # Linux: tools/setup-linux.sh; or single parts: rasm idsk cpcse
make
```

| Part | Source | Build |
|---|---|---|
| Homebrew | - | `brew install cmake ninja sdl2 sdcc` (SDCC 4.6.0 from Homebrew) |
| RASM | `EdouardBERGE/rasm` tag `v3.3.3` | `make`, copy `rasm.exe` to `opt/rasm/rasm` |
| iDSK | `cpcsdk/idsk` tag `v0.20` | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build` |
| CPCSyntaxError | `WacKEDmaN/CPCSyntaxError` tag `v0.16.0` | `cmake -S . -B build/cpcse -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/cpcse`, copy `cpcse`, `cpcse-ctl`, `roms/`, `sounds/`; macOS: first `git apply tools/patches/cpcse-hidden-window-no-vsync.patch` (for `make shot`) |

Not from upstream, but part of the kit repo: `lib/m4` (our C library for the M4 board: HTTP, DNS,
TCP sockets, written from Duke's m4info.txt), `lib/cpc` (SDCC `crt0.s`, firmware printf/keyboard)
and `tools/cpcbin.py` (`.ihx` -> binary with AMSDOS header). CPCSyntaxError brings the CPC ROMs
and Duke's M4 ROM. The emulated M4's SD card is the kit's `m4sd/` folder, its WiFi is the Mac's
network.

**Run the client:**

```sh
cd clients/cpc-tcp
make SERVER=127.0.0.1            # RGBTCP.BIN + rgbtcp.dsk
make sd                          # copy RGBTCP.BIN onto the emulated M4 SD card
cd ../../devkits/development-kit-cpc-and-m4 && make run
```

On the CPC: `RUN"RGBTCP"`.

Pitfalls:
- After a rebuild, `make sd` again - the emulator runs the copy on its SD card (`m4sd/`).
- `RUN` of a binary resets the CPC when it ends; to return to BASIC use
  `MEMORY &3FFF:LOAD"NAME":CALL &4000`.
- The emulated M4 answers ~10x faster than real hardware: check timing on a real CPC + M4.
- Headless runs/screenshots: `make shot CMD='RUN"RGBTCP"'` (real time, hidden window; on macOS
  only with the kit's no-vsync patch, which `setup-macos.sh` applies - without it the emulator stalls).

## 4. Apple II + FujiNet

**Build:**

```sh
cd devkits/development-kit-apple2-and-fujinet
tools/setup-macos.sh             # Linux: tools/setup-linux.sh; or single parts: cc65 fujinet-lib cadius prodos fngo
```

| Part | Source | Build |
|---|---|---|
| Homebrew | - | `brew install cmake ninja sdl3 boost gh` |
| cc65 | `cc65/cc65` master | as for the Atari |
| fujinet-lib | release `v5.1.1` | `gh release download v5.1.1 -R FujiNetWIFI/fujinet-lib -p "fujinet-lib-apple2-*.zip"` |
| cadius | `mach-kernel/cadius` tag `1.4.5` | `make` |
| ProDOS 2.4.3 | `ProDOS-8/ProDOS8-Releases` (`ProDOS_2_4_3.po` via raw.githubusercontent.com) | first 1024 bytes = boot blocks; `cadius EXTRACTFILE ... /PRODOS.2.4.3/PRODOS` |
| FujiNet Go Apple II | `FujiNetWIFI/fujinet-go-apple2-desktop` tag `v0.10.0` | `cmake -B build/fngo -G Ninja -DFRONTEND=macos -DCMAKE_BUILD_TYPE=Release && cmake --build build/fngo`, copy the `.app` |

FujiNet Go is AppleWin with the FujiNet firmware built in - no separate FujiNet-PC.

**Run the client:**

```sh
cd clients/apple2-tcp
make SERVER=127.0.0.1            # rgbtcp.po: ProDOS boot disk with RGBTCP.SYSTEM
cd ../../devkits/development-kit-apple2-and-fujinet
make run DISK=../../clients/apple2-tcp/rgbtcp.po
```

In CONFIG: host **SD** -> `rgbtcp.po` -> slot **1** -> **ESC** boots it.

Pitfalls:
- ProDOS 2.4.3 states no licence: `rgbtcp.po` (which contains it) and the kit's `opt/prodos`
  are never committed.
- Target `apple2enh` with `-C apple2enh-system.cfg`; call `network_init()` first (finds the
  FujiNet on the SmartPort).
- cadius writes no boot blocks - the Makefile `dd`s them from the ProDOS disk.
- No remote control: CONFIG is driven by hand.

## Real hardware

The C clients ask for the server at the start: RETURN keeps the default, or type `HOST` or
`HOST:PORT` (IP address or host name). After a failed connection they ask again, and **S** after a
match changes it. `make SERVER=192.168.17.138` sets the default - handy for an event, where
everyone just presses RETURN. Copy the image to the FujiNet's / M4's SD card. Ready-made images for the Classic Computing server are in `disk-images/`
(see its README). None of the TCP clients has run on real hardware yet.

## Thanks

Every tool in these kits is someone else's open-source work - cc65, fujinet-lib, FujiNet-PC,
atari800, FastBasic, VICE/vice-fn, SDCC, RASM, iDSK, CPCSyntaxError, Duke's M4 board, cadius,
ProDOS 8, FujiNet Go Apple II/AppleWin and more. The full list with links is in the README's
"Thanks" section. A warm thank you to all their maintainers!
