# assets/

Local image and font files for the server, referenced by relative path from the project root
(see `server/config.py`) - the server runs unchanged on any machine without adjusting paths.

## assets/logos/

Company logos + club logo (`LOGO_DIR` / `LOGO_NAME_CANDIDATES` in `server/config.py`). Filename per platform (first matching
extension wins: .png, .jpg, .jpeg, .gif, .bmp, either upper- or
lowercase):

- `atari.*` — Atari
- `commodore.*` or `c64.*` — Commodore 64
- `schneider.*`, `cpc.*` or `amstrad.*` — Schneider/Amstrad CPC
- `apple2.*` or `apple.*` — Apple II
- `logo.*` — the event's club logo (top right, always visible)

Company logos are reloaded on every new match (no restart needed).
The club logo is loaded only once at server startup.

`adam.jpg`, `msx.jpg` and `tandy.jpg` are for platforms without a client yet (not used by the
server). `logos/projects/` holds the logos of FujiNet, Meatloaf, M4 and WiC64 for the waiting
screen's tribute page (`TRIBUTE_ENTRIES` in `server/config.py`).

The logos are trademarks of their owners, used here only to show which computer is playing.

## assets/test_ftproot/

Sample content for a local test FTP/HTTP server for the photo download (fotofix, `PHOTO_SOURCE` in
`server/config.py`): the server fetches `<PIN>/photo.jpg`, so a visitor with the PIN `MUSTER` gets
`MUSTER/photo.jpg` - here a self-drawn placeholder. Not read by the server directly.

## assets/font/

The `.ttf` file for the demo scroll text, see `SCROLL_FONT_PATH` in
`server/config.py`. The Flynn font is not included (its licence doesn't
allow offering it for download) - see `assets/font/README.md` for the
link. Without it the server uses pygame's default font.
