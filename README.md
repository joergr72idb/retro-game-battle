# Retro Game Battle

Multi-game successor to [Retro Tron Battle](../retro-tron-battle) (built
for Classic Computing 2026): real retro computers (Atari XL/XE, Commodore 64 and Apple II via
FujiNet, Schneider/Amstrad CPC via an M4 board) steer via joystick over the network while a Python/pygame
server on a modern PC draws the whole board and runs the game logic.

New here: the **first connecting client picks which game to play** from
a server-provided list; the second client is auto-matched into it. New
games are added as plugins (`server/games/<name>.py`) without touching
the shared transport/pairing/rendering code or any existing client.

Games in this pass: **Tron** (ported rule-for-rule from Retro Tron
Battle) and **Snake** (new - a food-eating duel using the same
grid/tick model). See `docs/protocol.md` for the full wire protocol and
`server/games/base.py` for the plugin interface.

## Running the server

```
cd server
python3 game_battle_server.py [host] [tcp_port] [http_port]
# defaults: 0.0.0.0 6502 8080
# add --headless to skip the pygame window (useful for bot-only testing)
```

Requires `pip install pygame` unless run with `--headless`.

## Testing without hardware

`bots/game_bot.sh [HOST] [PORT] [PLATFORM] [NAME] [VARIANT] [WANT_GAME] [TURN_CHANCE] [PIN]`
simulates a client over TCP, including the game-selection step - see the
script's own header comment for examples (including how to exercise the
compatibility-filtering behavior with a `C64-WIC64` variant).

## Docs

- `docs/protocol.md` - the wire protocol (source of truth)
- `docs/pap_diagrams.pdf` - PAP flowcharts for explaining the games to
  visitors (page 1) and the server logic (page 2); regenerate with
  `docs/pap_src/build.sh` from the `.dot` sources there

## Clients

All clients speak the raw TCP protocol (`docs/protocol.md`, "Protocol 1"): one connection per
match, a `MOVE` only when the stick changes, `FIRE` on the button (Artillery).

- `clients/atari/game_battle_atari_client.bas` - Atari XL/XE + FujiNet (Atari BASIC)
- `clients/c64-tcp/` - C64 + FujiNet (C, cc65 + fujinet-lib)
- `clients/cpc-tcp/` - Schneider/Amstrad CPC + M4 board (C, SDCC + the M4 library)
- `clients/apple2-tcp/` - Apple //e enhanced or later + FujiNet (C, cc65 + fujinet-lib)

The C clients are built with the development kits next to this repository
(`../development-kit-c64-and-fujinet`, `../development-kit-cpc-and-m4`,
`../development-kit-apple2-and-fujinet`), which also run them in emulators (VICE + FujiNet-PC,
CPCSyntaxError with M4, FujiNet Go Apple II). Ready-made disk images: `disk-images/`.

The HTTP-polling BASIC clients (C64 Meatloaf, C64 WiC64, CPC `|HTTPMEM`) were dropped on
2026-11-14: they waited up to 0.3-1.3 s per tick for the network while the server runs 250 ms
ticks. They are in the git history (and in retro-tron-battle). The server keeps its HTTP bridge
(port 8080), so such clients still work against it.

The hardware/platform lessons in
[`retro-tron-battle/CLAUDE.md`](../retro-tron-battle/CLAUDE.md) (PETSCII case-folding, reserved
BASIC keywords, joystick port quirks, `RUN`-not-`GOTO` restarts, and so on) still apply.

## Thanks

Hans has developed the fotofix idea - many thanks from here!

## Not in this pass

- Pong and a Gorilla-style artillery game (see `docs/protocol.md`'s
  "Adding a new game" section for why Gorilla in particular needs a
  different input model).
- The Apple II BASIC draft of the reference repo wasn't carried over - `clients/apple2-tcp/`
  is a new client in C.
