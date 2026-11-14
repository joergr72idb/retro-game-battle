# Disk images

One folder per platform: `atari/`, `c64/`, `cpc/` (Schneider/Amstrad).
The client images are named `rgb.<ext>` (Atari C client: `rgbtcp.xex`).

The C clients' (Atari, C64, CPC, Apple II) default server is **192.168.17.138:6502** (the Classic Computing server). At the
start they show it and ask: RETURN keeps it, or type another `HOST[:PORT]` - so the images work
with any server. `make SERVER=<ip>` in the client's folder changes only the default.

## `cpc/rgb.dsk`

Schneider/Amstrad CPC + M4 disk image with the TCP client `RGBTCP.BIN` (C, `clients/cpc-tcp/`).
Load with `RUN"RGBTCP"` - or copy `RGBTCP.BIN` onto the M4's SD card. Regenerate:

```
cd clients/cpc-tcp && make SERVER=192.168.17.138 && cp rgbtcp.dsk ../../disk-images/cpc/rgb.dsk
```

## `c64/rgb.d64`

C64 + FujiNet disk image with the TCP client `rgbtcp` (C, `clients/c64-tcp/`). Load with
`LOAD"RGBTCP",8`, then `RUN`. Needs FujiNet firmware (network device 16), not Meatloaf.
Regenerate:

```
cd clients/c64-tcp && make SERVER=192.168.17.138 && cp rgbtcp.d64 ../../disk-images/c64/rgb.d64
```

## `atari/rgbtcp.xex`

The Atari C client (`clients/atari-tcp/`). No disk image needed: copy it to the FujiNet's SD card,
mount it in CONFIG like a disk and boot - the FujiNet loads XEX files itself. Rebuild:

```
cd clients/atari-tcp && make SERVER=192.168.17.138 && cp rgbtcp.xex ../../disk-images/atari/rgbtcp.xex
```

## `atari/rgb.atr`

Atari DOS 2.0S boot disk for FujiNet, based on the reference project's
`n-handler.atr`: `DOS.SYS`, `AUTORUN.SYS` (the `N:` handler, loads on
boot), the FujiNet `N*.COM`/`CONFIG`/`COPY` tools, and the client as
`RGB.LST` (ATASCII listing, no tokenized `.BAS`). After booting into
BASIC: `ENTER"D:RGB.LST"`, then `SAVE"D:RGB.BAS"` once, `RUN`.
Regenerate the client file on it:

```
python3 tools/atr_dos2_put.py disk-images/atari/rgb.atr \
    --put clients/atari/game_battle_atari_client.bas RGB.LST
```
