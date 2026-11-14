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

## Fixes carried over from the reference (2026-09-28)

Ported 1:1 from retro-tron-battle, where the details and the hardware
verification are documented (its CLAUDE.md, CPC section): CPC screen
saver (client build 3), key-buffer drain for joystick taps + no
per-tick buffer clearing (build 4), `MEMORY &7FFF` for the HTTP buffer
(build 5), and the server `drain_moves()` fix that reads all queued
MOVEs per tick (server build 6). Verified on real hardware there only -
this repo's own CPC client has not run on hardware yet.

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

## Ported from the reference after the fork (2026-09-26)

- **Atari input-latency equalizer** (`ATARI_LATENCY_EQUALIZER` and
  friends in `server/config.py`, applied in `engine/runner.py`'s
  `drain_moves()`) - same idea and starting values as the reference, see
  its CLAUDE.md. Here it holds back the whole Atari MOVE (both
  `direction` and the raw `input_queue` entry), so it applies to every
  game, not just Tron. `FIRE` is **not** delayed (the reference has no
  `FIRE`); revisit if Atari turns out to have an edge in Artillery.
  Not yet playtested.
- **Dynamic pairwise latency equalizer** (server build 7, 2026-09-28,
  `DYNAMIC_LATENCY_EQUALIZER`/`EQ_*` in `server/config.py`,
  `setup_equalizer()` in `engine/runner.py`) - ported from the reference's
  server build 18, same behavior and values, see its CLAUDE.md section of
  the same name. Only difference: `runner.py` finds HTTP players by their
  `tick_gaps` attribute instead of `isinstance(HTTPPlayerConn)` (importing
  `net_http` there would be a cycle via `pairing.py`). The measurement
  starts at GAME START, so time spent in the game menu doesn't count.
  Like the static rule, it delays MOVE only, not `FIRE`. The reference
  build was playtested as fair on real hardware on 2026-10-01 (all three
  pairings, see its CLAUDE.md); this port itself not yet playtested.
- **Stale waiting player check** (server build 8, 2026-10-12,
  `stale_waiting_reason()`/`drop_stale_waiting()` in `engine/pairing.py`,
  `WAITING_HTTP_STALE_S` in `server/config.py`) - ported from the
  reference's server build 19, see its CLAUDE.md section "Stale waiting
  player". Before every pairing (immediate match and after the menu
  selection) dead entries are removed from `waiting_players`: same
  platform + name joining again, closed connection, or an HTTP player
  without `/tick` for 5 s. HTTP players are recognized by their
  `joined_at` attribute (same import-cycle reason as above). Headless
  test with simulated clients only (same four cases as the reference).
- Fourth waiting-screen view (fotofix/greeting thank-you) removed, along
  with its image asset - cycle is now scroll -> highscore -> tribute.
- `[move]` log lines colored on the console (`MOVE_LOG_COLOR`).
- Hans has developed the fotofix idea - many thanks from here!

## CPC over TCP and the Pong reverse fix (2026-11-14, branch cpc-tcp-client)

- **`clients/cpc-tcp/`** - a C client (SDCC + the M4 library of `../development-kit-cpc-and-m4`)
  that speaks the raw TCP protocol like the Atari, instead of HTTP polling. Reason: on real
  hardware `|HTTPMEM` blocks ~550 ms per tick against the server's 250 ms ticks. See its README.
- **`clients/c64-tcp/`** - the same for the C64 with a FujiNet (cc65 + fujinet-lib, network device
  16), built with `../development-kit-c64-and-meatloaf`. Converts PETSCII<->ASCII on the wire.
  Tested in VICE + FujiNet-PC.
- **Pong bug, all platforms:** `_apply_move()`'s reverse guard (Tron: no turning back) also applied
  to Pong, so after U a D was ignored (and vice versa) - a paddle could only change direction via
  L/R, which stops it. Fixed with `GameModule.reverse_guard` (default True; Pong False), copied
  onto `Player.reverse_guard` in `run_game()`. Tron still ignores reversals (bot match checked).
  Never noticed before because the bots change direction at random.
- **Emulator testing for the CPC works now:** CPCSyntaxError with M4 emulation
  (`../development-kit-cpc-and-m4`) runs the BASIC client (`|HTTPMEM`) unchanged and the TCP
  client. Caveat: the emulated M4 answers in ~50 ms per HTTP tick, ten times faster than real
  hardware - timing and the equalizer still need real-hardware checks.
- The server needs **Python 3.10+** (`float | None` in `engine/model.py`): on macOS use a Homebrew
  Python, not the system 3.9.

## HTTP clients dropped, FIRE everywhere (2026-11-14)

- `clients/c64/` (Meatloaf, WiC64 + driver) and `clients/cpc/` (|HTTPMEM) are gone - replaced by
  the C TCP clients. They are in the git history. The server's HTTP bridge (port 8080, `net_http.py`)
  stays, so the protocol and its docs remain valid.
- No client sent `FIRE` before, so Artillery could only be fired by bots. Now: Atari BASIC
  (`STRIG`, lines 302-306, before line 370's jump), C64/CPC C clients (button or Space).
- `disk-images/c64/rgb.d64` and `cpc/rgb.dsk` hold the C clients built for 192.168.17.138.

## Apple II (2026-11-14)

- **`clients/apple2-tcp/`** - Apple //e enhanced + FujiNet (SmartPort), C with cc65 + fujinet-lib,
  a ProDOS SYS program on a ProDOS 2.4.3 boot disk. Built and run with
  `../development-kit-apple2-and-fujinet` (FujiNet Go Apple II emulator). Won a match there.
- Snake and Pong now allow the `APPLE2` variant (Tron/Artillery had no restriction) - the
  server has to be restarted to pick that up.
- `rgbtcp.po` is not committed: it contains ProDOS 2.4.3, whose GitHub release repo states no licence.

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
