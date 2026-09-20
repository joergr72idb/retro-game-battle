# Retro Game Battle

Multi-game successor to [Retro Tron Battle](../retro-tron-battle) (built
for Classic Computing 2026): real retro computers (Atari XL/XE via
FujiNet, Commodore 64 via Meatloaf or WiC64, Schneider/Amstrad CPC via an
M4 board) steer via joystick over the network while a Python/pygame
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

## Clients

- `clients/atari/game_battle_atari_client.bas` - Atari XL/XE + FujiNet (TCP)
- `clients/c64/game_battle_c64_client.bas` - C64 + Meatloaf (HTTP polling)
- `clients/c64/game_battle_c64_wic64_client.bas` - C64 + WiC64 (HTTP polling, experimental)
- `clients/cpc/game_battle_cpc_client.bas` - Schneider/Amstrad CPC + M4 (HTTP polling)

All four were extended from Retro Tron Battle's clients with one new
step: after connecting, if offered a game list, show it and send a
choice back before anything else changes. Transferring these to real
hardware (petcat tokenizing, WinAPE/AMSDOS headers, FujiNet disk images,
etc.) works exactly as documented in
[`retro-tron-battle/CLAUDE.md`](../retro-tron-battle/CLAUDE.md) - that
document's hardware/platform lessons (PETSCII case-folding, reserved
BASIC keywords, joystick port quirks, `RUN`-not-`GOTO` restarts, and so
on) all still apply here and aren't repeated in this repo.

## Not in this pass

- Pong and a Gorilla-style artillery game (see `docs/protocol.md`'s
  "Adding a new game" section for why Gorilla in particular needs a
  different input model).
- The Apple II client (an untested draft in the reference repo) wasn't
  carried over.
