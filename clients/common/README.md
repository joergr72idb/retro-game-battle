# The shared part of the C clients

`rgb_core.c` is the protocol and game flow of all four C clients (`../atari-tcp`, `../c64-tcp`,
`../cpc-tcp`, `../apple2-tcp`): server prompt, PIN, game menu, `MOVE`/`FIRE`, reading the server's
lines without blocking. It uses fujinet-lib's network calls (`network_open`, `network_status`,
`network_read_nb`, `network_write`, `network_close`):

- Atari, C64, Apple II: FujiNet's [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib)
- CPC + M4: [fujinet-network-cpc](../../lib/fujinet-network-cpc), the same calls on the M4

What differs per machine is in each client's `platform.c` and `platform.h` (screen, keyboard,
joystick, texts, PETSCII on the C64) - the interface is `rgb_platform.h`. Each client's Makefile
compiles `rgb_core.c` with its own `platform.h` (`-I .`).

A new client for another FujiNet machine needs only a `platform.c`/`platform.h` and a Makefile.
