# Disk images

## `rgbclient.dsk`

Schneider/Amstrad CPC disk image with the current CPC client
(`RGB.BAS`, build 2), stored as a headerless ASCII file - load it with
`RUN"RGB"` (AMSDOS detects ASCII automatically; loading is a bit slower
than tokenized BASIC). Regenerate after changing the client source:

```
python3 tools/cpc_dsk_put.py disk-images/rgbclient.dsk \
    clients/cpc/game_battle_cpc_client.bas RGB.BAS
```
