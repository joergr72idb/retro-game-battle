# Retro Game Battle

Multi-game successor to [Retro Tron Battle](https://github.com/joergr72idb/retro-tron-battle) (built
for Classic Computing 2026): real retro computers (Atari XL/XE, Commodore 64 and Apple II via
FujiNet, Schneider/Amstrad CPC via an M4 board) steer via joystick over the network while a Python/pygame
server on a modern PC draws the whole board and runs the game logic.

New here: the **first connecting client picks which game to play** from
a server-provided list; the second client is auto-matched into it. New
games are added as plugins (`server/games/<name>.py`) without touching
the shared transport/pairing/rendering code or any existing client.

Games: **Lightcycle** (Tron, ported rule-for-rule from Retro Tron Battle), **Snake duel**,
**Pong** and **Artillery duel** (turn-based: aim with the stick, fire with the button). See
`docs/protocol.md` for the full wire protocol and `docs/howto-new-game.md` for writing a new game.

## The games

What the server's screen shows during a match - the retro computers only steer:

| Lightcycle (Tron) | Snake duel |
|---|---|
| ![Lightcycle](docs/screenshots/games/tron.png) | ![Snake duel](docs/screenshots/games/snake.png) |
| Steer your lightcycle, its trail stays. Whoever hits a wall or a trail first loses. | Eat the food to grow; hit a wall, yourself or the other snake and you lose. |
| **Pong** | **Artillery duel** |
| ![Pong](docs/screenshots/games/pong.png) | ![Artillery duel](docs/screenshots/games/artillery.png) |
| Up/down moves the paddle. First to 5 points wins. | Turn-based: up/down sets the angle, left/right the power, the button fires. A hit wins. |

The platform logos, names and colours come from the clients that are playing; a photo appears when
a visitor enters a PIN (fotofix). These pictures were drawn by the real server and game code, with
two scripted players instead of real clients (`tools/game_screenshots.py`).

## Running the server

Needs **Python 3.10 or newer** (on macOS not the system Python 3.9 - e.g. Homebrew's
`python@3.13`) and pygame:

```
python3 -m venv .venv
.venv/bin/pip install pygame
.venv/bin/python server/game_battle_server.py [host] [tcp_port] [http_port]
# defaults: 0.0.0.0 6502 8080
# add --headless to skip the pygame window (useful for bot-only testing)
```

Settings (grid, tick rate, colours, photo source, latency equalizer, ...) are in
`server/config.py`, marked `<-- EDIT`. Results go to `game_results.csv` and the log to
`game_battle_server.log` in the repo root. The scroll font is not included - see
`assets/font/README.md`.

## Testing without hardware

`bots/game_bot.sh [HOST] [PORT] [PLATFORM] [NAME] [VARIANT] [WANT_GAME] [TURN_CHANCE] [PIN] [FIRE_CHANCE]`
simulates a client over TCP, including the game-selection step - see the
script's own header comment for examples (including how to exercise the
compatibility-filtering behavior with a `C64-WIC64` variant).

## Docs

- `docs/protocol.md` - the wire protocol (source of truth)
- `docs/howto-new-game.md` - how to write a new game for the server (interface, example, testing)
- `docs/howto-devkits.md` - how to build the four development kits (Atari, C64, CPC, Apple II)
  and run each client in its emulator
- `devkits/` - the four development kits themselves (build scripts, configs, our M4 library, samples)
- `docs/pap_diagrams.pdf` - PAP flowcharts for explaining the games to
  visitors (page 1) and the server logic (page 2); regenerate with
  `docs/pap_src/build.sh` from the `.dot` sources there

## Clients

All clients speak the raw TCP protocol (`docs/protocol.md`, "Protocol 1"): one connection per
match, a `MOVE` only when the stick changes, `FIRE` on the button (Artillery).

- `clients/atari/game_battle_atari_client.bas` - Atari XL/XE + FujiNet (Atari BASIC)
- `clients/atari-tcp/` - Atari XL/XE + FujiNet (C, cc65 + fujinet-lib; boots directly as an XEX)
- `clients/c64-tcp/` - C64 + FujiNet (C, cc65 + fujinet-lib)
- `clients/cpc-tcp/` - Schneider/Amstrad CPC + M4 board (C, SDCC + the M4 library)
- `clients/apple2-tcp/` - Apple //e enhanced or later + FujiNet (C, cc65 + fujinet-lib)

Game menu of each client (emulators, server on 127.0.0.1):

| Atari XL + FujiNet (C) | Atari XL + FujiNet (BASIC) |
|---|---|
| ![Atari C client](docs/screenshots/atari-c.png) | ![Atari BASIC client](docs/screenshots/atari.png) |
| **C64 + FujiNet** | **CPC + M4** |
| ![C64 client](docs/screenshots/c64.png) | ![CPC client](docs/screenshots/cpc.png) |
| **Apple II + FujiNet** | |
| ![Apple II client](docs/screenshots/apple2.png) | |

The C clients are built with the development kits in `devkits/` (Atari, C64, CPC, Apple II), which
also run them in emulators (atari800 + FujiNet-PC, VICE + FujiNet-PC, CPCSyntaxError with M4,
FujiNet Go Apple II). Each client has a step-by-step `BUILD.md`; ready-made images are in
`disk-images/` (no Apple II image - it would contain ProDOS). How to build the kits and run each
client in its emulator: [`docs/howto-devkits.md`](docs/howto-devkits.md).

The HTTP-polling BASIC clients (C64 Meatloaf, C64 WiC64, CPC `|HTTPMEM`) were dropped on
2026-11-14: they waited up to 0.3-1.3 s per tick for the network while the server runs 250 ms
ticks. They are in the git history (and in [Retro Tron Battle](https://github.com/joergr72idb/retro-tron-battle)). The server keeps its HTTP bridge
(port 8080), so such clients still work against it.

The hardware/platform lessons in
[Retro Tron Battle's `CLAUDE.md`](https://github.com/joergr72idb/retro-tron-battle/blob/master/CLAUDE.md) (PETSCII case-folding, reserved
BASIC keywords, joystick port quirks, `RUN`-not-`GOTO` restarts, and so on) still apply.

## Thanks

Hans has developed the fotofix idea - many thanks from here!

### The projects our development kits are built on

None of the clients would exist without the open-source work of these projects. Our kits
(`devkits/`) only pin, build and wire them together - the real work is theirs. A warm thank you
to all their maintainers and contributors!

**Compilers, assemblers, libraries**
- [cc65](https://github.com/cc65/cc65) - C compiler and ca65 assembler for the 6502 (Atari, C64, Apple II)
- [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) - the C library for the FujiNet
- [FastBasic](https://github.com/dmsc/fastbasic) - a fast, modern BASIC for the Atari
- [SDCC](https://sdcc.sourceforge.net/) ([sources](https://sourceforge.net/projects/sdcc/)) - the Z80 C compiler for the CPC
- [RASM](https://github.com/EdouardBERGE/rasm) - Z80 assembler
- [iDSK](https://github.com/cpcsdk/idsk) - CPC disc images
- [cadius](https://github.com/mach-kernel/cadius) - ProDOS disk images (originally by Brutal Deluxe)
- [ProDOS 8 releases](https://github.com/ProDOS-8/ProDOS8-Releases) - ProDOS 2.4.3 for the Apple II boot disk

**FujiNet, M4 and the emulators**
- [FujiNet firmware / FujiNet-PC](https://github.com/FujiNetWIFI/fujinet-firmware) - the FujiNet for the Atari (NetSIO) and the Commodore bus (NetIEC)
- [FujiNet Go Apple II](https://github.com/FujiNetWIFI/fujinet-go-apple2-desktop) - Apple II emulator with FujiNet built in, on [AppleWin](https://github.com/AppleWin/AppleWin) ([FujiNet's AppleWin fork](https://github.com/FujiNetWIFI/AppleWin))
- [atari800](https://github.com/atari800/atari800) - Atari 8-bit emulator with NetSIO
- [vice-fn](https://github.com/dillera/vice-fn) - the FujiNet fork of [VICE](https://vice-emu.sourceforge.io/) ([VICE-Team/svn-mirror](https://github.com/VICE-Team/svn-mirror)) with the NetIEC bridge
- [CPCSyntaxError](https://github.com/WacKEDmaN/CPCSyntaxError) - CPC emulator with M4 board emulation
- The [M4 board](http://www.spinpoint.org/) by [Duke](https://github.com/M4Duke) - its ROM and the documented command interface ([m4info.txt](http://www.spinpoint.org/cpc/m4info.txt)) our `lib/m4` is written from

**Underneath**
- [Homebrew](https://github.com/Homebrew/brew), [SDL](https://github.com/libsdl-org/SDL),
  [Mbed TLS](https://github.com/Mbed-TLS/mbedtls), [CMake](https://github.com/Kitware/CMake),
  [Ninja](https://github.com/ninja-build/ninja), [Boost](https://github.com/boostorg/boost),
  [GNU Bison](https://www.gnu.org/software/bison/), [xa](https://www.floodgap.com/retrotech/xa/),
  [Python](https://github.com/python/cpython) and [pygame](https://github.com/pygame/pygame) for the server

## Not in this pass

- The Apple II BASIC draft of the reference repo wasn't carried over - `clients/apple2-tcp/`
  is a new client in C.
