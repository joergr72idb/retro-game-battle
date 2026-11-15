# C64 client over TCP (FujiNet) - C

The C64 client (it replaced the BASIC HTTP clients `clients/c64/`, Meatloaf and WiC64, on 2026-11-14),
speaking the raw TCP protocol the Atari/FujiNet client uses (`docs/protocol.md`, "Protocol 1").

**Why:** the HTTP clients wait for a full request per tick (Meatloaf ~300 ms, WiC64 ~1280 ms) while
the server runs 250 ms ticks. This client keeps one TCP connection open through the FujiNet's
network device (IEC device 16, fujinet-lib), sends `MOVE` the moment the stick changes, and reads
the server's lines without blocking.

**Hardware:** a C64 with a FujiNet on the IEC bus - for example the LOLIN D32 Pro "Meatloaf" board
flashed with FujiNet's `fujinet-iec-lolin-d32` firmware (same wiring). Not a Meatloaf firmware:
Meatloaf has no FujiNet network device.

## Build

Step by step, from an empty Mac to a running client, with checks and troubleshooting:
**[BUILD.md](BUILD.md)**. In short:

Needs the C64 development kit (cc65, fujinet-lib >= 5.1.1, VICE with the FujiNet bridge): `devkits/development-kit-c64-and-fujinet` in this repo once its
setup script (`tools/setup-macos.sh` / `tools/setup-linux.sh`) has run, otherwise a checkout of the kit next to this repository. See
`docs/howto-devkits.md`.

```sh
make                            # rgbtcp.prg + rgbtcp.d64 for the server 127.0.0.1 (VICE on this Mac)
make SERVER=192.168.17.138      # default server for a real C64: the server's LAN address
make run                        # VICE + the kit's FujiNet-PC with rgbtcp.prg
```

At the start the client shows its server and asks: RETURN keeps it, or type `HOST` or `HOST:PORT`
(an IP address or a host name). After a failed connection it asks again; after a match,
**S** at "PRESS A KEY TO PLAY AGAIN" changes the server. The address from `make SERVER=` is only the
default.

On the C64: `LOAD"RGBTCP",8` and `RUN`. PIN, game menu (1-4), joystick in **port 2** or the cursor
keys to steer, fire button or **Space** to fire (Artillery). After the match: any key plays again.

## Notes

- cc65 turns string literals into PETSCII (upper and lower case swapped against ASCII): the client
  converts every line it sends to ASCII and every line it receives to PETSCII.
- Status: tested in VICE (dillera's FujiNet fork) + FujiNet-PC 2026-11-14 - HELLO, menu, START,
  MOVE (cursor keys), END/STATS, play again; Pong won. **Not yet run on real hardware.**
