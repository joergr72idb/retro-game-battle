# assets/

Local image and font files for the server (`server/tron_server.py`),
referenced by relative path from the project root — copy and commit
them there, then the server runs unchanged on any machine without
adjusting paths.

## assets/logos/

Company logos + club logo. Filename per platform (first matching
extension wins: .png, .jpg, .jpeg, .gif, .bmp, either upper- or
lowercase):

- `atari.*` — Atari
- `commodore.*` or `c64.*` — Commodore 64
- `schneider.*`, `cpc.*` or `amstrad.*` — Schneider/Amstrad CPC
- `apple2.*` or `apple.*` — Apple II (experimental, see CLAUDE.md)
- `logo.*` — the event's club logo (top right, always visible)

Company logos are reloaded on every new match (no restart needed).
The club logo is loaded only once at server startup.

## assets/font/

The `.ttf` file for the demo scroll text, see `SCROLL_FONT_PATH` in
`server/config.py`. The Flynn font is not included (its licence doesn't
allow offering it for download) - see `assets/font/README.md` for the
link. Without it the server uses pygame's default font.
