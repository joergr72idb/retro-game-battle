# Retro Game Battle - wire protocol

This is the maintained protocol reference (the equivalent of
`retro-tron-battle/server/tron_server.py`'s own top-of-file docstring, kept
here as a separate doc instead since this server's protocol code is split
across several files under `server/engine/`). Update this file whenever the
protocol changes - it's the source of truth, not the individual `.bas`
client files.

## Design

Same as retro-tron-battle: retro computers only steer, the server draws
the board. New here: the first connecting client picks which game to
play from a server-provided list; the second client is auto-matched into
it with no menu shown. See `server/games/base.py` for the game-plugin
interface and `server/engine/pairing.py` for the selection/pairing logic.

## Protocol 1: raw TCP (port 6502: Atari/FujiNet, and the C clients for CPC/M4 and C64/FujiNet)

```
Client -> Server:
    HELLO <PLATFORM> <NAME> <VARIANT> [PIN]
    GAME <n>                       game selection (see below)
    MOVE <U|D|L|R>
    BYE

Server -> Client:
    WAIT GAMES <n> <id1>:<label1> <id2>:<label2> ...
                                    sent to the FIRST connection instead of
                                    a bare WAIT - already filtered to games
                                    this client's VARIANT is allowed to play
    WAIT                            sent once a game has been chosen and
                                    there's still no opponent
    ERR INCOMPATIBLE GAMES <n> ...  sent when joining rejects this client
                                    for the already-waiting game (pairing-
                                    time only, see below) - same list
                                    format, filtered for this client
    ERR TOO MANY INVALID SELECTIONS sent after 5 invalid GAME <n> replies,
                                    connection is then closed
    START <game-id> <w> <h> <x1> <y1> <x2> <y2> <yourplayernum> <p1plat> <p2plat>
    END WIN <PLATFORM> <NAME>   |   END DRAW
    STATS GAMES <n> <PLATFORM> <n> ... DRAWS <n>   (per game, not global)
```

`VARIANT` is the compatibility id, separate from the display `PLATFORM` -
e.g. Meatloaf and WiC64 both display as `C64` but have different variants
(`C64` vs `C64-WIC64`) since they have very different network latency (see
`retro-tron-battle/CLAUDE.md`'s tick-latency measurements). Send `-` or
omit the segment to default `VARIANT` to `PLATFORM`.

### Game selection

Whoever connects when nobody is waiting for any game becomes the
"chooser" and gets `WAIT GAMES ...` instead of a bare `WAIT`. It replies
with `GAME <n>` (n = 1-based position in the list it was just sent) on
the same connection/session it already has open - no new phase, no new
connection. An invalid or unparseable `<n>` gets the same list re-sent
(bounded to 5 retries, then the connection is closed). A valid selection
either pairs it immediately (if someone's already waiting for that exact
game) or makes it the new waiting player for that game (plain `WAIT`).

A second/later connection whose variant is compatible with an
already-waiting game is paired immediately - no menu. If its variant
is NOT compatible with the (only) waiting game, it gets
`ERR INCOMPATIBLE GAMES ...` instead (filtered to what it CAN play) and
should send a fresh `GAME <n>` from that list, or give up/restart. The
already-waiting player is never affected by a rejected newcomer.

Filtering is symmetric and per-client: a client is only ever offered
games its variant is compatible with in the first place, so an
incompatible pairing can only happen in the one direction described
above (a second client trying to join a game it was never offered) - not
a full compatibility matrix.

### `START`'s shared prefix

`START <game-id> ...` always starts with the game id so a client that
doesn't care what game it is (all four current clients don't parse
`START`'s fields at all) needs no changes beyond the selection step
itself - `START`'s remaining fields are defined per game (Tron and Snake
currently share the same tail shape: grid dimensions + two spawn points).

## Protocol 2: HTTP polling (port 8080 - no client in this repo uses it since 2026-11-14)

```
GET /join/<platform>/<name>/<variant>/<pin-or-NONE>
    -> "SESSION <id>\n" + whatever's already queued (a WAIT GAMES list,
       a plain WAIT, or even a START if this connection was instantly
       auto-matched as the second player - this is the COMMON case for
       a second player, not an edge case, so a client MUST parse past
       the "SESSION <id>\n" prefix to see it, not just check the first
       few characters of the whole response)

GET /tick/<session>/<direction-or-GAMEn-or-N>
    -> whatever is queued since the last poll - same content as TCP's
       server-to-client lines. A direction (U/D/L/R) is translated to a
       MOVE; GAME<n> (e.g. GAME2) is a game selection, same semantics as
       TCP's `GAME <n>` command, just reusing the existing /tick slot
       instead of a new HTTP route; anything else (typically "N") queues
       nothing.
```

Session/mailbox semantics, grace period after game end, and
`ERR UNKNOWN SESSION` are unchanged from retro-tron-battle - see
`server/engine/net_http.py`.

## Compatibility

Declared as data per game module (`compatible_variants` on each
`GameModule` in `server/games/`), not hardcoded per-platform checks:

```
TRON:  no restriction (None)      - proven fine even at WiC64's ~1280ms/poll
SNAKE, PONG: {"ATARI", "C64", "CPC", "APPLE2"} - exclude C64-WIC64
```

The C clients in `clients/cpc-tcp/`, `clients/c64-tcp/` and `clients/apple2-tcp/` use this TCP
protocol with the plain `CPC` / `C64` / `APPLE2` variants, so they get the same games as the BASIC (HTTP) clients of their platform.

## Reverse guard

By default a `MOVE` opposite to the current direction is ignored (Tron can't turn back into its
own trail). A game module can switch this off with `reverse_guard = False` - Pong does, since a
paddle has to go up and down. Artillery reads the raw `input_queue` instead.

## Stats / CSV

One shared `game_results.csv` (not one file per game), with a `game`
column as the first field. Each registered game gets its own `Stats`
instance (games played / wins per platform / draws) - `STATS` lines on
the wire are per-game, never global.

## Adding a new game

Write `server/games/<name>.py` implementing the `GameModule` interface
(`server/games/base.py`): `id`, `label`, `compatible_variants`, optionally
`reverse_guard`, and `init_round`, `tick`, `start_message`,
`render_snapshot`, `draw_extra`. Register it in `server/games/__init__.py`.
Nothing under `server/engine/` or `server/display/` needs to change, and no
client needs changes as long as the game uses `MOVE` and `FIRE` (the menu is
server-generated) - but mind the menu line limits of the clients.

Step by step, with a complete example game and how to test it:
**[`docs/howto-new-game.md`](howto-new-game.md)**.
