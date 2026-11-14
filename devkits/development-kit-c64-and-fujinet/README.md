# Development Kit for the C64 and FujiNet

A self-contained toolchain on macOS for writing C64 programs that use the FujiNet, and testing them
in VICE with an emulated FujiNet on the serial bus. Everything is built into `opt/` inside this
folder; nothing is installed system-wide except a few Homebrew packages.

| Tool | Version | What it is |
|---|---|---|
| [cc65](https://github.com/cc65/cc65) | V2.19 (git) | C compiler (`cl65`) and the **ca65** assembler |
| [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) | 5.1.1 | C library for the FujiNet (network, clock, FujiNet device) |
| [VICE (FujiNet fork)](https://github.com/dillera/vice-fn) | trunk, 2026-11-01 | C64 emulator with real-device support for units 8–30, plus `petcat` and `c1541` |
| NetIEC bridge | same | `libopencbm.dylib`: sends the emulated serial bus over TCP to FujiNet-PC |
| [FujiNet-PC](https://github.com/FujiNetWIFI/fujinet-firmware) | master `8cc29e8b5` | FujiNet firmware for the Commodore bus (incl. the Meatloaf drive code) as a Mac program |

```
cl65 / petcat ──► build/*.prg ──► VICE x64sc ── bridge (libopencbm) ── TCP 1541 (NetIEC) ──► FujiNet-PC
                                                                    devices 16 network · 29 clock · 30 FujiNet
                                                                    drives 8-15 (Meatloaf) · web UI 127.0.0.1:8001
```

## Setup

```sh
tools/setup.sh                 # Homebrew packages + builds everything missing into opt/
tools/setup.sh vice            # rebuild single parts: cc65 fujinet-lib fujinet-pc vice
```

Versions are pinned at the top of `tools/setup.sh`. Build logs go to `build/logs/`.

## Use

```sh
make                           # samples: build/hello-c.prg, hello-asm.prg, hello-bas.prg + build/samples.d64
make run                       # FujiNet-PC + VICE with the C sample
make run PRG=build/hello-bas.prg
make run UNITS="8 16 29 30"    # also drive 8 from FujiNet-PC (Meatloaf drive) instead of VICE's own
make config                    # VICE with FujiNet as drive 8, for FujiNet's CONFIG: LOAD"CONFIG.PRG",8
make stop                      # stops this kit's FujiNet-PC
```

**FujiNet CONFIG on the C64:** pick a host with the cursor keys + RETURN, a file with RETURN, then
the drive slot (1–8 or cursor keys) and RETURN (read only) or W (read/write). The C64 **←** key
(Mac: **§** or **ß**) leaves CONFIG and boots the mounted disk.

Samples in `samples/`:
- `hello.c` – C with fujinet-lib: asks the FujiNet for its version, hostname and IP address.
- `hello.s` – ca65 assembler: prints a line through the KERNAL.
- `hello.bas` – C64 BASIC, tokenized with `petcat`.

Without a running emulator, `opt/vice-bridge/netiecctl` talks to FujiNet-PC directly:
`netiecctl status 8`, `netiecctl dir 8`, `netiecctl fuji e8` (adapter config).

## Notes

- **ASCII vs. PETSCII:** FujiNet sends ASCII; on the C64 the two cases are swapped and ASCII
  lowercase shows as graphics characters. Convert text from the FujiNet before printing
  (see `ascii2pet()` in `samples/hello.c`).
- **Meatloaf on FujiNet-PC** has the drive code, but not Meatloaf's HTTP, TNFS and `ML:` file
  systems (they need the ESP32's software). `OPEN 1,8,3,"http://..."` as used by the Meatloaf
  clients of retro-tron-battle / retro-game-battle(-ng) did **not** work here; network access goes
  through the FujiNet devices (fujinet-lib) instead.
- Only the standard IEC protocol: no JiffyDOS or other fast loaders on the FujiNet units.
- fujinet-lib programs built before 5.1.1 misread every reply from current firmware.
- **Ports:** web UI `127.0.0.1:8001`, NetIEC TCP 1541 (`WEB_PORT` / `NETIEC_PORT` in the Makefile).
  The Atari kit uses 8000 and UDP 9997, so both kits run side by side.
- The web UI has no login; it listens on 127.0.0.1 only, so it isn't reachable from the network.
- **Keyboard:** `keymaps/c64-mac-de.vkm` = VICE's German map plus the C64 ← key on §, ß and `.
- VICE's settings and log stay in `build/` (`-config build/vicerc`, `-logfile`), never in the home folder.
- **Real hardware:** the Meatloaf board (LOLIN D32 Pro) can also run FujiNet's `fujinet-iec-lolin-d32`
  firmware – same pins (ATN 32, CLK 33, DATA 25, SRQ 26, RESET 34).
