# Disk images

## `rgbclient.dsk`

Schneider/Amstrad CPC disk image with the current CPC client
(`RGB.BAS`, build 3), stored as a headerless ASCII file - load it with
`RUN"RGB"` (AMSDOS detects ASCII automatically; loading is a bit slower
than tokenized BASIC). Regenerate after changing the client source:

```
python3 tools/cpc_dsk_put.py disk-images/rgbclient.dsk \
    clients/cpc/game_battle_cpc_client.bas RGB.BAS
```

## `rgbclient.d64`

C64 disk image (for the Meatloaf SD card, or a real 1541 for the WiC64
variant): `rgb` (Meatloaf client), `rgbwic` (WiC64 client) and
`fotofix.c000` (the WiC64 driver `rgbwic` loads itself - see
[`clients/c64/wic64-driver/`](../clients/c64/wic64-driver/)). Load with
`LOAD"RGB",8` or `LOAD"RGBWIC",8`, then `RUN`. Regenerate (VICE tools):

```
petcat -w2 -o rgb.prg -- clients/c64/game_battle_c64_client.bas
petcat -w2 -o rgbwic.prg -- clients/c64/game_battle_c64_wic64_client.bas
c1541 -format "retrogamebattle,26" d64 disk-images/rgbclient.d64 \
    -write rgb.prg rgb -write rgbwic.prg rgbwic \
    -write clients/c64/wic64-driver/FOTOFIX.C000 fotofix.c000
```

## `rgbclient.atr`

Atari DOS 2.0S boot disk for FujiNet, based on the reference project's
`n-handler.atr`: `DOS.SYS`, `AUTORUN.SYS` (the `N:` handler, loads on
boot), the FujiNet `N*.COM`/`CONFIG`/`COPY` tools, and the client as
`RGB.LST` (ATASCII listing, no tokenized `.BAS`). After booting into
BASIC: `ENTER"D:RGB.LST"`, then `SAVE"D:RGB.BAS"` once, `RUN`.
Regenerate the client file on it:

```
python3 tools/atr_dos2_put.py disk-images/rgbclient.atr \
    --put clients/atari/game_battle_atari_client.bas RGB.LST
```
