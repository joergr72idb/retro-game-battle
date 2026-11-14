# Apple II client over TCP (FujiNet) - C

An Apple //e (enhanced) or later with a FujiNet on the SmartPort, speaking the raw TCP protocol the
Atari/FujiNet client uses (`docs/protocol.md`, "Protocol 1"). A ProDOS SYS program
(`RGBTCP.SYSTEM`) on a ProDOS boot disk (`rgbtcp.po`), which starts by itself.

## Build

Step by step, from an empty Mac to a bootable disk, with checks and troubleshooting:
**[BUILD.md](BUILD.md)**. In short:

Needs the Apple II development kit (cc65, fujinet-lib >= 5.1.1, cadius, ProDOS 2.4.3): `devkits/development-kit-apple2-and-fujinet` in this repo once its
`tools/setup.sh` has run, otherwise a checkout of the kit next to this repository. See
`docs/howto-devkits.md`.

```sh
make                            # rgbtcp.po for the server 127.0.0.1 (the emulator on this Mac)
make SERVER=192.168.17.138      # default server for a real Apple II: the server's LAN address
```

At the start the client shows its server and asks: RETURN keeps it, or type `HOST` or `HOST:PORT`
(an IP address or a host name). After a failed connection it asks again; after a match,
**S** at "PRESS A KEY TO PLAY AGAIN" changes the server. The address from `make SERVER=` is only the
default.

In FujiNet's CONFIG: mount `rgbtcp.po` in slot 1 and boot (ESC). PIN, game menu (1-4), joystick or
arrow keys to steer, button 0 or **Space** to fire (Artillery). After the match: any key plays again.

## Notes

- `network_init()` first: on the Apple II it finds the FujiNet's network device on the SmartPort.
- The boot disk contains ProDOS 2.4.3 (`PRODOS` + boot blocks, from the ProDOS-8 project's GitHub
  releases). That repository states no licence, so `rgbtcp.po` is not committed here - build it.
- Status: tested in FujiNet Go Apple II (AppleWin core + built-in FujiNet) 2026-11-14: menu, a
  match, win (build 1). Build 2 (server prompt) is built but not yet run in the emulator - the
  same code works in the C64 and CPC clients. **Not yet run on real hardware.**
