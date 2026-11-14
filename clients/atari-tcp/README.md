# Atari XL/XE client over TCP (FujiNet) - C

An Atari XL/XE with a FujiNet, speaking the raw TCP protocol (`docs/protocol.md`, "Protocol 1") -
the same as the Atari BASIC client `clients/atari/`, but in C: it talks to the FujiNet's network
device directly over SIO (fujinet-lib), needs no DOS and no `N:` handler, and the FujiNet boots it
directly as `rgbtcp.xex`.

## Build

Needs the Atari development kit (cc65, fujinet-lib, atari800 with NetSIO, FujiNet-PC):
`devkits/development-kit-atari-xl-and-fujinet` in this repo once its `tools/setup.sh` has run,
otherwise a checkout of the kit next to this repository. See `docs/howto-devkits.md`.

```sh
make                            # rgbtcp.xex for the server 127.0.0.1 (the emulator on this Mac)
make SERVER=192.168.17.138      # default server for a real Atari: the server's LAN address
make run                        # the kit's FujiNet-PC boots rgbtcp.xex in atari800 (like a real FujiNet)
make config                     # the kit's FujiNet-PC back to its CONFIG menu at boot
```

At the start the client shows its server and asks: RETURN keeps it, or type `HOST` or `HOST:PORT`
(an IP address or a host name). After a failed connection it asks again; after a match,
**S** at "PRESS A KEY TO PLAY AGAIN" changes the server. The address from `make SERVER=` is only the
default.

On a real Atari: copy `rgbtcp.xex` to the FujiNet's SD card (or a TNFS server), mount it in
FujiNet's CONFIG like a disk and boot - the FujiNet loads XEX files itself. PIN, game menu (1-4),
joystick 1 or CTRL+arrow keys to steer, button or **Space** to fire (Artillery).

## Notes

- Built with `-t atari`, not `-t atarixl`: fujinet-lib calls the OS SIO vector, which the atarixl
  runtime switches out.
- The joystick is read from the hardware (`PORTA`, `TRIG0`), not from the OS copies `STICK0`/`STRIG0`:
  the OS skips updating those while SIO is running, and the client talks to the FujiNet nearly all
  the time - the copies would hardly ever change.
- SIO's "noisy I/O" is switched off (`SOUNDR` = 0), otherwise the constant polling buzzes.
- In atari800 (`make run`), the Mac's cursor keys and Space are joystick 1.
- `make run` copies `rgbtcp.xex` onto the kit's FujiNet SD card and mounts it in drive slot 1
  (`fn_boot.py` edits `fnconfig.ini`, CONFIG off), so the FujiNet boots it like on real hardware.
  Not `atari800 -run`: with a disk in D1: (e.g. DOS + `N:` handler for the BASIC client) the Atari
  boots that first, the program overwrites the handler, and the keyboard stops reacting.
  `make config` brings back FujiNet's CONFIG menu.
- Status: played in atari800 + FujiNet-PC (2026-11-14): server prompt, PIN, menu, steering, match.
  **Not yet run on real hardware.**
