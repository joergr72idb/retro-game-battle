# Schneider CPC client over TCP (M4 board) - C

The CPC client (it replaced the BASIC HTTP client `clients/cpc/` on 2026-11-14), speaking the raw TCP protocol the
Atari/FujiNet client uses (`docs/protocol.md`, "Protocol 1") instead of HTTP polling.

**Why:** the BASIC client polls the HTTP bridge with `|HTTPMEM`, which blocks the CPC for each
request - about 550 ms per tick on real hardware, while the server runs 250 ms ticks
(`TICK_RATE = 4`), so a real CPC could only steer about every second tick. This client keeps one
TCP connection open (M4 sockets), sends `MOVE` the moment the stick changes, and reads the server's
lines without blocking.

## Build

Needs the CPC development kit next to this repository (`../development-kit-cpc-and-m4`: SDCC,
`lib/cpc`, `lib/m4`, iDSK).

```sh
make                            # RGBTCP.BIN + rgbtcp.dsk for the server 127.0.0.1 (the emulator)
make SERVER=192.168.17.138      # for a real CPC: the server's LAN address (compiled in)
make sd                         # copy RGBTCP.BIN onto the kit's emulated M4 SD card
```

On the CPC: `RUN"RGBTCP"`. PIN, then the game menu (1-4), joystick or cursor keys to steer,
fire button or **Space** to fire (Artillery).
After the match: any key plays again.

## Status

- Tested in the CPCSyntaxError emulator (2026-11-14) against the server and `bots/game_bot.sh`:
  HELLO, game menu, WAIT, START, MOVE, END/STATS and play-again all work; Pong won and lost.
- **Not yet run on real hardware.** The M4's real socket latency over WiFi is unmeasured - the
  server treats TCP players as 0 ms in the equalizer.
