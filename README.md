# Development Kit for Atari XL and FujiNet

A self-contained toolchain for macOS and Linux for writing Atari 8-bit XL/XE programs that use the FujiNet,
and testing them in an emulator with an emulated FujiNet. Everything is built into `opt/` inside
this folder; nothing is installed system-wide except a few Homebrew or apt packages.

| Tool | Version | What it is |
|---|---|---|
| [cc65](https://github.com/cc65/cc65) | V2.19 (git) | C compiler (`cl65`) and the **ca65** assembler |
| [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) | 5.1.0 | C library for the FujiNet (network, SD card, clock, …) |
| [FastBasic](https://github.com/dmsc/fastbasic) | 4.8.1 | Modern structured BASIC, cross-compiled to `.xex` |
| [atari800](https://github.com/atari800/atari800) | 7.2.1 | Emulator with NetSIO (needed to talk to FujiNet-PC) |
| [FujiNet-PC](https://github.com/FujiNetWIFI/fujinet-firmware) | 1.6.1 | The FujiNet firmware as a Mac/Linux program |

```
cl65 / fastbasic ──► build/*.xex ──► atari800 -netsio ── UDP 9997 ──► FujiNet-PC
                                                          SD card: opt/fujinet-pc/SD
                                                          web UI:  http://127.0.0.1:8000
```

## Setup

```sh
tools/setup-macos.sh           # macOS: Homebrew packages + builds everything missing into opt/
tools/setup-linux.sh           # Linux (Debian/Ubuntu): checks the apt packages, then the same
tools/setup-linux.sh atari800  # rebuild single parts: cc65 fujinet-lib atari800 fastbasic fujinet-pc
```

Linux needs these packages first (`setup-linux.sh` lists the missing ones and stops):
`sudo apt install build-essential git autoconf automake cmake pkg-config libsdl2-dev libmbedtls-dev python3-venv gh unzip curl`

Versions are pinned at the top of both setup scripts (keep them in step). Build logs go to `build/logs/`.

## Use

```sh
make                           # builds the samples: build/hello-c.xex, hello-asm.xex, hello-fb.xex
make run                       # FujiNet-PC + atari800 with the C sample
make run XEX=build/hello-fb.xex
make run JOY=1                 # with joystick 0 on the cursor keys, fire = Space
make stop                      # stops this kit's FujiNet-PC
```

Samples in `samples/`:
- `hello.c` – C with fujinet-lib: asks the FujiNet for its version and IP address.
- `hello.s` – ca65 assembler: prints a line through CIO.
- `hello.bas` – FastBasic.

`opt/fastbasic/fastbasic.atr` is the FastBasic IDE for the Atari itself (boot it as a disk).

## Notes

- Compile with `-t atari`, not `-t atarixl`: fujinet-lib calls the OS SIO vector, which the
  atarixl runtime switches out (the call hangs).
- Start FujiNet-PC before the emulator (`make run` does that). Each emulated power-on reboots
  FujiNet-PC; `tools/run-fujinet` restarts it automatically.
- **Ports:** web UI `127.0.0.1:8000`, NetSIO UDP 9997 (`WEB_PORT` / `NETSIO_PORT` in the Makefile).
  The C64 kit uses 8001 and TCP 1541, so both kits run side by side. Other FujiNet-PCs with the same
  ports (e.g. the [Berlin Blues BBS](https://github.com/joergr72idb/atari-fujinet-berlin-blues-bbs)'s own instance: 8000 + 9997) can't run at the same time as this one.
- The web UI has no login; it listens on 127.0.0.1 only, so it isn't reachable from the network.
- **ROMs:** `atari800.cfg` uses the original Atari ROMs from `rom/` (not in git, copyrighted):
  `REV02.ROM` = 800XL OS rev. 2 (default), plus 400/800 rev. A PAL/B NTSC, 1200XL rev. 10,
  600XL rev. 1, 800XE rev. 3, XEGS rev. 4 and BASIC A/B/C (CRC-32 list: `rom/OSROMS.TXT`).
  Without them atari800 falls back to its built-in AltirraOS. atari800 is built with
  `--disable-download`, so it never fetches ROMs from the internet on its own.

## Using the kit from other projects

```make
DEV      ?= ../development-kit-atari-xl-and-fujinet
CL65     := $(DEV)/opt/cc65/bin/cl65
FNLIB    := $(DEV)/opt/fujinet-lib
ATARI800 := $(DEV)/opt/atari800/bin/atari800
export CC65_HOME := $(DEV)/opt/cc65/share/cc65
```

The folder layout under `opt/` matches the `atari-dev` folder the [Berlin Blues BBS](https://github.com/joergr72idb/atari-fujinet-berlin-blues-bbs) expects
(`make DEV=../development-kit-atari-xl-and-fujinet`).

## Built on the work of others

This kit is only glue: setup scripts that download and build other people's software at fixed
versions, plus Makefiles and settings that wire it together. Everything that does the real work
comes from these projects - used unmodified. **A warm thank you to all their maintainers and
contributors!**

- [cc65](https://github.com/cc65/cc65) - the C compiler and ca65 assembler
- [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) - the C library for the FujiNet
- [FastBasic](https://github.com/dmsc/fastbasic) by Daniel Serpell - the modern BASIC, cross-compiler and Atari IDE
- [atari800](https://github.com/atari800/atari800) - the emulator, with NetSIO for FujiNet-PC; it brings
  [AltirraOS](https://www.virtualdub.org/altirra.html) by Avery Lee, the free OS used when no original ROMs are present
- [FujiNet firmware](https://github.com/FujiNetWIFI/fujinet-firmware) - built here as FujiNet-PC, the FujiNet as a Mac/Linux program
- the [FujiNet project](https://fujinet.online) as a whole - the hardware this kit is about

**Underneath:** [Homebrew](https://github.com/Homebrew/brew) or Debian/Ubuntu packages, [SDL](https://github.com/libsdl-org/SDL),
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls), [CMake](https://github.com/Kitware/CMake),
[pkgconf](https://github.com/pkgconf/pkgconf), [GNU Autoconf](https://www.gnu.org/software/autoconf/) and
[Automake](https://www.gnu.org/software/automake/).
