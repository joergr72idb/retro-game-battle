# CLAUDE.md - Retro Game Battle

This project was ported/generalized from
[`retro-tron-battle`](../retro-tron-battle), which remains the deployed
build for Classic Computing 2026 and is **not** touched by this project.

**Read [`retro-tron-battle/CLAUDE.md`](../retro-tron-battle/CLAUDE.md)
first** for every hardware/platform-specific lesson (PETSCII
case-folding on Meatloaf, reserved BASIC keywords per dialect, C64
joystick-port ghosting, the `RUN`-not-`GOTO` restart pattern, AMSDOS
headers on the CPC, FujiNet TCP vs HTTP `AUX2` differences, and so on).
None of that is repeated here - it all still applies unchanged, since
the clients in this repo are direct extensions of that project's
clients, not rewrites.

## What's actually new here

- `docs/protocol.md` - the wire protocol, including the parts that
  didn't exist in the reference (game selection, compatibility
  filtering, per-game `START`/`STATS`).
- `server/` is split into `engine/` (transport, pairing, the generic
  match runner, stats/CSV, render-state plumbing - all ported near-
  verbatim from `tron_server.py`) and `games/` (one file per game,
  implementing `games/base.py`'s `GameModule` interface). See
  `docs/protocol.md`'s "Adding a new game" section before writing a new
  one.
- `display/pygame_loop.py` is the same HUD/panels/waiting-screen code as
  the reference, with the one Tron-shaped piece (trail drawing) split
  into a generic `draw_body()` plus a per-game `draw_extra()` hook.

## One bug found and fixed while porting (not present in the reference)

The reference's HTTP `/join` response is `"SESSION <id>\n" + <whatever
was already queued>` - for a SECOND player that gets instantly
auto-matched, that queued content can be a `START ...` line, not just
`WAIT`. The reference C64/WiC64/CPC clients' "did we get a START"
check (`LEFT$(R$,5)="START"`) was applied to the WHOLE response
(`R$`), which still starts with `"SESSION..."` at that point - so the
check could never actually match, and this case fell through to
printing "waiting for opponent" even when the game had, in fact, already
started. In the reference this is a rare, likely-unnoticed edge case
(needs two players joining at nearly the same instant). In this project
it stops being rare: the very same response now commonly carries a
`WAIT GAMES` list too, which every client MUST correctly parse past the
`SESSION` prefix to see. Fixed in all three HTTP-polling clients here by
extracting the content after the session line into its own variable
(`RS$`/`rs$`) and running every status check against that instead of the
raw response - see the `rs$`/`RS$` construction right after session-id
extraction in each `.bas` file.

## Known BASIC-dialect gotcha applied here for the first time in this codebase

Commodore and Locomotive BASIC's `STR$()` reserves a leading space for
the sign on non-negative numbers (e.g. C64's `STR$(3)` = `" 3"`, not
`"3"`). The reference project never used `STR$()` for anything
protocol-critical, so this never came up before. It matters here because
the new `GAME <n>` selection is sent as a literal HTTP URL path segment
(`/tick/<session>/GAME<n>`) on the HTTP-polling clients - an embedded
space there breaks the HTTP request line itself. Fixed by stripping it:
`NS$=MID$(STR$(NSEL),2)` (or `ns$=mid$(str$(nsel),2)`) before building
the URL, in the Meatloaf, WiC64 and CPC clients. The Atari (TCP) client
sends its selection as a plain text line instead of a URL segment, and
the server-side parser already tolerates surrounding whitespace there
(`.strip()`/`.split()`), so this particular fix wasn't needed for it -
but keep it in mind if `STR$()` is ever used for something URL-bound on
any client in the future.

## Testing status

- **Engine (all of `server/engine/` and `server/games/`):** exercised
  end-to-end with `bots/game_bot.sh` against a running `--headless`
  server - Tron parity, a full Snake match, per-game `STATS`/CSV
  separation, the compatibility-filtering scenario (WiC64 excluded from
  Snake, `ERR INCOMPATIBLE` on pairing, the original waiting player
  unaffected, a later compatible bot still pairs into it), and the
  bounded invalid-selection retry (5 retries then disconnect) all
  verified working as designed.
- **Clients (`clients/*.bas`):** verified statically only - line-number
  ascending/no-duplicate check (per the reference project's own
  convention), Atari line-length limit, and `petcat` tokenization for
  both C64 clients (all pass). **Not yet run on any emulator or real
  hardware** - do that next, the same way the reference project does it
  (see its CLAUDE.md's "Testing without real hardware" section:
  Fujisan/atari800 for Atari, VICE for C64/Meatloaf+WiC64, CPCEmu
  (unverified even there) for CPC/M4).
