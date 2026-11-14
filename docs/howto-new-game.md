# How to: write a new game for the server

The server is split into a generic **engine** (connections, pairing, game menu, countdown, tick
loop, win/draw, statistics, display) and **games** - one Python file per game in `server/games/`.
A new game is one new file plus two lines in `server/games/__init__.py`. Nothing in
`server/engine/` or `server/display/` has to change, and usually **no client either**: the clients
are just controllers (stick + fire button), the board is only drawn on the server's screen, and the
game menu is sent by the server.

This page walks through the interface, what the engine does for you, and a complete example game
(a coin race), which was run against the server with two bots while writing this.

## The idea in one picture

```
client  --HELLO-->  pairing (engine/pairing.py)  --menu: WAIT GAMES ...-->  client picks GAME <n>
                                |
                    two players with the same game
                                v
run_game(p1, p2, game)  (engine/runner.py)
   game.init_round(p1, p2)                      set up positions, board
   send game.start_message(..)  to both         "START <ID> ..."
   versus screen, countdown 3-2-1
   every 1/TICK_RATE s (250 ms):
       read MOVE/FIRE/BYE -> p.direction, p.input_queue, p.fired, p.alive
       game.tick(p1, p2)                        move, collide, score, decide
       render: p.trail, (p.x, p.y), game.render_snapshot(..)
   until p1.alive and p2.alive is no longer true
   exactly one alive -> END WIN <platform> <name>, both dead -> END DRAW, then STATS
```

## The interface (`server/games/base.py`)

A game is a class with these attributes and methods (registered as one instance):

| Member | What it is |
|---|---|
| `id` | Wire id, e.g. `"TRON"`. Upper case, no spaces, no `:`. Used in `WAIT GAMES`, `START`, the statistics and the CSV. Unique. |
| `label` | Menu label, e.g. `"LIGHTCYCLE"`. **One token** - no spaces (use `-`). Shown on 40-column screens, keep it short. |
| `compatible_variants` | `None` = every client may play it; or a set of variant ids, e.g. `{"ATARI", "C64", "CPC", "APPLE2"}`. Other clients don't see the game in their menu. |
| `reverse_guard` | Optional, default `True`: a `MOVE` opposite to the current direction is ignored (Tron can't turn back into its own trail). Set `False` when going back must be possible (Pong paddles, a free-moving player). |
| `init_round(p1, p2)` | Start of a match: set `x`, `y`, `direction`, `trail`, `alive` of both players and create the board state. |
| `tick(p1, p2)` | One step (4 times a second): move, collide, score. To end the match, set `alive = False` on the loser (or on both for a draw). No return value. |
| `start_message(p1, p2, playernum)` | The `START` line for player 1 or 2. Must begin with `START <id>`. |
| `render_snapshot(p1, p2)` | Extra data for the screen, as a dict (or `{}`), e.g. `{"food": (x, y)}`. Ends up in `state["game_extra"]`. |
| `draw_extra(screen, state, draw_ctx)` | Draws that extra data with pygame (or does nothing). |

The four existing games are the best reference, from simple to unusual:

- `tron.py` - the minimum: a trail as a set, standard `START`, nothing extra to draw.
- `snake.py` - the trail as an ordered list, shared board state (food), `draw_extra`.
- `pong.py` - `reverse_guard = False`, scores, a ball as float coordinates, paddles drawn through `trail`.
- `artillery.py` - turn-based: reads the raw `input_queue` and `fired` instead of `direction`.

## What the players give you

Each player is an `engine.model.Player`. In `tick()` you use:

| Field | Meaning |
|---|---|
| `x`, `y` | Position on the grid (`0 <= x < config.GRID_W`, `0 <= y < config.GRID_H`, 50 x 30). The engine draws a highlighted cell there (the "head"). |
| `direction` | `"U"`, `"D"`, `"L"` or `"R"` - the last direction steered (the client sends `MOVE` only when the stick changes; the direction stays until the next one). `config.DIRS[d]` gives `(dx, dy)`. |
| `input_queue` | Every `MOVE` letter received, unfiltered, in order (also the ones `reverse_guard` ignored). For games where U/D/L/R are commands rather than steering (Artillery: angle/power). **Clear it yourself** after reading. |
| `fired` | `True` after a `FIRE` (button or Space on all clients). **Reset it yourself** to `False` once handled. |
| `alive` | Set to `False` to end the match (see below). Also `False` when a client disconnects or sends `BYE`. |
| `trail` | The cells the engine draws in the player's colour: a `set` or a `list` of `(x, y)`. Use it for anything that "belongs" to a player - a trail, a snake body, a paddle, a single marker cell. |
| `game_state` | Free slot for your board state. Pattern: one object shared by both players (`p1.game_state = p2.game_state = state`). |
| `platform`, `name` | For the `START` line and your own logs. |

You can also attach your own attributes (`p1.coins = 0`, Pong's `p.score`, Artillery's
`p.angle`/`p.power`) - set them in `init_round()`, since a player object only exists for one match.

## Ending a match

The engine only knows "alive": after every tick it checks `p1.alive and p2.alive`.

- One player loses (crash, opponent reached the score, was hit): `loser.alive = False`.
- Both lose in the same tick: both `False` -> `END DRAW`.
- A score-based game decides the same way - Pong and the coin race below set the loser's `alive` to
  `False` when the winner reaches the target score.

The result, the per-game statistics (`STATS GAMES ...`) and the CSV row (`game_results.csv`, first
column = your `id`) are written by the engine.

## What the clients get

Clients only need to understand this much, and they already do:

- the menu line `WAIT GAMES <n> <id>:<label> ...` - they show the labels and send `GAME <n>`;
- a line starting with `START` - the match runs; from here they send `MOVE U/D/L/R` when the stick
  changes and `FIRE` on the button;
- `END ...` and `STATS ...` - shown as text.

The rest of the `START` line is yours to define, but no current client reads it - keep the common
format (`START <id> <w> <h> <x1> <y1> <x2> <y2> <playernum> <p1plat> <p2plat>`) unless a client
really needs something else. **If your game needs a new kind of input**, that's a protocol and client
change: see `docs/protocol.md` first.

**Limits of the clients' menu** (all games a client may play, in one line):

- At most **9 games** - the clients read the choice as one key, `1`-`9`.
- The Atari BASIC client keeps the line in `A$`, dimensioned to 80 characters (`DIM A$(80)` in its
  line 30). With the four current games the line is **exactly 80** characters long
  (`WAIT GAMES 4 TRON:LIGHTCYCLE SNAKE:SNAKE-DUEL PONG:PONG ARTILLERY:ARTILLERY-DUEL`). Everything
  after character 80 is lost, so the BASIC client can't show a fifth game correctly. Enlarge `A$`
  in `clients/atari/game_battle_atari_client.bas` (and test it) when you add a game.
- The C clients keep up to 199 (Atari, C64, Apple II) and 255 (CPC) characters - longer lines are
  dropped and the client would wait forever. That's room for about nine games with short ids and
  labels.

## Drawing

The display (`display/pygame_loop.py`) draws each frame from a snapshot of the render state:

1. the grid, then each player's `trail` in their platform colour with a white frame around `(x, y)`;
2. then your `draw_extra(screen, state, draw_ctx)`.

`state["game_extra"]` is what your `render_snapshot()` returned in the last tick; `state["pos1"]` /
`state["pos2"]` are the players' positions. `draw_ctx` has `pygame`, `field_x` (left edge of the
board in pixels), `hud_height` (top edge) and `cell` (cell size in pixels). Cell `(gx, gy)` starts at
pixel `(field_x + gx * cell, hud_height + gy * cell)`.

Two rules, because the display runs in its own thread:

- `render_snapshot()` must return **values, not live objects** - tuples, numbers, strings, or new
  lists/dicts. The display reads them while the next tick already runs.
- `draw_extra()` only draws. It never changes game state, and it isn't called at all with
  `--headless`.

## Example: a coin race

Both players steer one cell around the board (no trail), a coin appears somewhere, whoever reaches it
first scores. First to 5 coins wins. Walls don't kill - you stop at the edge. Save as
`server/games/coins.py`:

```python
"""
Coin race: both players steer a single cell, whoever reaches the coin first scores.
First to WIN_COINS wins. Walls don't kill - you just stop at the edge.
"""

import random
from dataclasses import dataclass

import config

DIRS = config.DIRS
WIN_COINS = 5


@dataclass
class CoinsRoundState:
    coin: tuple                      # (x, y) of the current coin


class CoinsGame:
    id = "COINS"                     # wire id: upper case, no spaces or colons
    label = "COIN-RACE"              # menu label: one token, no spaces
    compatible_variants = None       # every client may play it
    reverse_guard = False            # U right after D is a normal move here

    def init_round(self, p1, p2):
        y = config.GRID_H // 2
        p1.x, p1.y, p1.direction = 5, y, "R"
        p2.x, p2.y, p2.direction = config.GRID_W - 6, y, "L"
        p1.alive = p2.alive = True
        p1.coins = p2.coins = 0
        state = CoinsRoundState(coin=self._new_coin(p1, p2))
        p1.game_state = p2.game_state = state      # one shared object for the board
        self._sync_trails(p1, p2)

    def _new_coin(self, p1, p2):
        while True:
            c = (random.randrange(config.GRID_W), random.randrange(config.GRID_H))
            if c not in ((p1.x, p1.y), (p2.x, p2.y)):
                return c

    def _sync_trails(self, p1, p2):
        # one cell per player: the engine draws `trail` with the generic body renderer
        p1.trail = {(p1.x, p1.y)}
        p2.trail = {(p2.x, p2.y)}

    def tick(self, p1, p2):
        state = p1.game_state
        for p in (p1, p2):
            dx, dy = DIRS[p.direction]
            p.x = max(0, min(config.GRID_W - 1, p.x + dx))
            p.y = max(0, min(config.GRID_H - 1, p.y + dy))

        on1 = (p1.x, p1.y) == state.coin
        on2 = (p2.x, p2.y) == state.coin
        if on1 != on2:                               # both on it at once: nobody scores
            (p1 if on1 else p2).coins += 1
        if on1 or on2:
            state.coin = self._new_coin(p1, p2)

        # the engine ends the match when someone is no longer alive
        if p1.coins >= WIN_COINS:
            p2.alive = False
        elif p2.coins >= WIN_COINS:
            p1.alive = False
        self._sync_trails(p1, p2)

    def start_message(self, p1, p2, playernum):
        return (f"START {self.id} {config.GRID_W} {config.GRID_H} "
                f"{p1.x} {p1.y} {p2.x} {p2.y} {playernum} {p1.platform} {p2.platform}")

    def render_snapshot(self, p1, p2):
        state = p1.game_state
        return {"coin": state.coin if state else None,
                "coins": (getattr(p1, "coins", 0), getattr(p2, "coins", 0))}

    def draw_extra(self, screen, state, draw_ctx):
        pygame, cell = draw_ctx["pygame"], draw_ctx["cell"]
        coin = state.get("game_extra", {}).get("coin")
        if coin:
            cx = draw_ctx["field_x"] + coin[0] * cell + cell // 2
            cy = draw_ctx["hud_height"] + coin[1] * cell + cell // 2
            pygame.draw.circle(screen, (255, 210, 0), (cx, cy), cell // 2 - 1)
```

Register it in `server/games/__init__.py` - the menu order is the registration order:

```python
from games.coins import CoinsGame
...
_register(CoinsGame())
```

That's all. Restart the server and the C clients show a fifth entry `COIN-RACE` (the Atari BASIC
client needs a larger `A$` first - see "Limits" above).

## Testing

Use the project's Python (3.10+, see the README) and keep test results out of the real statistics:
the server writes `game_results.csv` and `game_battle_server.log` in the repo root - copy them away
before a test run and back afterwards, or delete the test rows.

**1. The logic alone**, without network, from `server/`:

```python
import sys; sys.path.insert(0, ".")
from engine.model import Player
import games

g = games.GAME_REGISTRY["COINS"]
p1, p2 = Player(None, None, "ATARI", "A"), Player(None, None, "C64", "B")
g.init_round(p1, p2)
print(g.start_message(p1, p2, 1))

def steer(p, coin):                       # a greedy "player": straight to the coin
    if p.x != coin[0]:
        p.direction = "R" if coin[0] > p.x else "L"
    else:
        p.direction = "D" if coin[1] > p.y else "U"

ticks = 0
while p1.alive and p2.alive and ticks < 5000:
    steer(p1, p1.game_state.coin); steer(p2, p1.game_state.coin)
    g.tick(p1, p2); ticks += 1
print(ticks, p1.coins, p2.coins, p1.alive, p2.alive, g.render_snapshot(p1, p2))
```

**2. Over the network with two bots** - a headless server on spare ports, so a running server isn't
disturbed:

```sh
cd server && ../.venv/bin/python game_battle_server.py 127.0.0.1 6602 8092 --headless
# second terminal, from bots/: two bots that both want the new game
./game_bot.sh 127.0.0.1 6602 ATARI BotA ATARI COINS 4
./game_bot.sh 127.0.0.1 6602 C64   BotB C64   COINS 4
```

The server prints `Games available: ..., COINS` at the start, then `GAME START ... - COINS - ...`
and, when one bot stops (Ctrl+C), `GAME END ... wins!`; the other bot gets `END WIN ...` and
`STATS ...`. The bots steer at random (the last argument is how often they turn), so a match to 5
coins can take long - stopping one bot is the quick way to see a match end. To test `compatible_variants`, start a bot with a variant
that isn't allowed (`./game_bot.sh ... C64 BotC C64-WIC64 SNAKE`): it must not be offered the game.

**3. With the window** (without `--headless`) to check `draw_extra`, and finally with a real client
in an emulator - see `docs/howto-devkits.md`.

## Checklist

- [ ] `server/games/<name>.py` with `id`, `label`, `compatible_variants`, the five methods (and
      `reverse_guard = False` if moving back must be possible)
- [ ] `id` unique, upper case, no spaces/colons; `label` one token, short
- [ ] `init_round()` sets position, direction, `trail` and `alive` of both players and all own attributes
- [ ] `tick()` ends the match only through `alive`; clears `input_queue` / resets `fired` if it uses them
- [ ] `render_snapshot()` returns values, not live objects
- [ ] registered in `server/games/__init__.py`
- [ ] menu line still fits the clients (9 games, Atari BASIC `A$`, see "Limits")
- [ ] tested: logic alone, two bots, with the window, one real client
- [ ] `docs/protocol.md` updated if the `START` line or the input changed
