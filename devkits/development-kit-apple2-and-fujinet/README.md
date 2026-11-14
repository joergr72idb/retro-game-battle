# Development Kit for the Apple II and FujiNet

A self-contained toolchain on macOS for Apple II programs that use the FujiNet, tested in an emulator
with FujiNet built in. Everything is built into `opt/` inside this folder.

| Tool | Version | What it is |
|---|---|---|
| [cc65](https://github.com/cc65/cc65) | V2.19 (git) | C compiler and ca65 assembler (`-t apple2enh`) |
| [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) | 5.1.1 | C library for the FujiNet (SmartPort) |
| [cadius](https://github.com/mach-kernel/cadius) | 1.4.5 | ProDOS disk images (`.po`) |
| [ProDOS](https://github.com/ProDOS-8/ProDOS8-Releases) | 2.4.3 | `PRODOS` + boot blocks for boot disks (no licence stated: never committed) |
| [FujiNet Go Apple II](https://github.com/FujiNetWIFI/fujinet-go-apple2-desktop) | 0.10.0 | Emulator (GPLv3): AppleWin core + FujiNet firmware in one app, boots into CONFIG |

## Setup and use

```sh
tools/setup.sh                       # Homebrew packages + everything missing into opt/
make run DISK=path/to/disk.po        # put the disk on the emulated FujiNet's SD card, start the emulator
```

In CONFIG: host **SD** → the image → drive slot **1** → **ESC** boots it.

A cc65 program for ProDOS: `cl65 -t apple2enh -C apple2enh-system.cfg` gives a SYS file; on a disk
with `PRODOS` and the ProDOS boot blocks, the first `*.SYSTEM` file starts by itself (see
`clients/apple2-tcp/Makefile` in [Retro Game Battle](https://github.com/joergr72idb/retro-game-battle)). Call `network_init()` before other fujinet-lib
network calls - on the Apple II it finds the network device on the SmartPort.

## Notes

- The emulator keeps settings and data in `build/xdg-config` and `build/xdg-data` (`XDG_*`), not in
  the home folder. Its FujiNet SD card: `build/xdg-data/fujinet-go-apple2/fujinet/SD`.
- No remote control: CONFIG and the program are driven by hand. FujiNet's log is
  `build/xdg-data/fujinet-go-apple2/fujinet/fujinet-console.log`.
- Ports 1985 and 64001 (127.0.0.1) - no conflict with the Atari (8000, UDP 9997), C64 (8001, 1541)
  and CPC (8765) kits or the game server (6502, 8080).
- FujiNet's own `autorun.po` boots without ProDOS: its boot block loads `CONFIG.SYSTEM` by name, so it
  can't be reused for other programs.

## Built on the work of others

This kit is only glue: a setup script that downloads and builds other people's software at fixed
versions, plus Makefiles and settings that wire it together. Everything that does the real work
comes from these projects - used unmodified. **A warm thank you to all their maintainers and
contributors!**

- [cc65](https://github.com/cc65/cc65) - the C compiler and ca65 assembler
- [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) - the C library for the FujiNet
- [cadius](https://github.com/mach-kernel/cadius) - ProDOS disk images (originally by Brutal Deluxe)
- [ProDOS 8](https://github.com/ProDOS-8/ProDOS8-Releases) - ProDOS 2.4.3 for the boot disks (downloaded, never
  redistributed: the release repository states no licence)
- [FujiNet Go Apple II](https://github.com/FujiNetWIFI/fujinet-go-apple2-desktop) - the emulator, built on
  [AppleWin](https://github.com/AppleWin/AppleWin) ([FujiNet's AppleWin fork](https://github.com/FujiNetWIFI/AppleWin))
  with the [FujiNet firmware](https://github.com/FujiNetWIFI/fujinet-firmware) built in
- the [FujiNet project](https://fujinet.online) as a whole - the hardware this kit is about

**Underneath:** [Homebrew](https://github.com/Homebrew/brew), [SDL](https://github.com/libsdl-org/SDL),
[Boost](https://github.com/boostorg/boost), [CMake](https://github.com/Kitware/CMake) and
[Ninja](https://github.com/ninja-build/ninja).
