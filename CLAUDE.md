# CLAUDE.md - Retro Game Battle

This project was ported/generalized from
[Retro Tron Battle](https://github.com/joergr72idb/retro-tron-battle) (locally `../retro-tron-battle`), which remains the deployed
build for Classic Computing 2026 and is **not** touched by this project.

**Read [Retro Tron Battle's `CLAUDE.md`](https://github.com/joergr72idb/retro-tron-battle/blob/master/CLAUDE.md)
first** for every hardware/platform-specific lesson (PETSCII
case-folding on Meatloaf, reserved BASIC keywords per dialect, C64
joystick-port ghosting, the `RUN`-not-`GOTO` restart pattern, AMSDOS
headers on the CPC, FujiNet TCP vs HTTP `AUX2` differences, and so on).
None of that is repeated here. It applied 1:1 to the BASIC clients this repo started with; since
2026-11-14 the clients are the Atari BASIC client plus four C clients (see the sections below),
and the BASIC-dialect lessons matter mostly for the Atari one.

**Current state in short:** server with four games (Tron, Snake, Pong, Artillery) as plugins;
clients `clients/atari/` (Atari BASIC) and `clients/{atari,c64,cpc,apple2}-tcp/` (C, raw TCP,
server entered at runtime); development kits in `devkits/`; how-tos in `docs/`. The sections below
are a log, oldest first - later sections override earlier ones.

## Fixes carried over from the reference (2026-09-28)

*(History: the CPC BASIC client these fixes were for was replaced by `clients/cpc-tcp/` on
2026-11-14. The server fix `drain_moves()` still applies.)*

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
  `docs/howto-new-game.md` before writing a new one.
- `display/pygame_loop.py` is the same HUD/panels/waiting-screen code as
  the reference, with the one Tron-shaped piece (trail drawing) split
  into a generic `draw_body()` plus a per-game `draw_extra()` hook.

## One bug found and fixed while porting (not present in the reference)

*(History: the HTTP-polling clients were dropped on 2026-11-14 - the fix is in the git history.
Any new HTTP client must still parse past the `SESSION` line, see `docs/protocol.md`.)*

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

*(History: concerned the HTTP-polling clients dropped on 2026-11-14; still valid for any BASIC
client that builds URLs with `STR$()`.)*

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

## CPC over TCP and the Pong reverse fix (2026-11-14)

- **`clients/cpc-tcp/`** - a C client (SDCC + the M4 library of `../development-kit-cpc-and-m4`)
  that speaks the raw TCP protocol like the Atari, instead of HTTP polling. Reason: on real
  hardware `|HTTPMEM` blocks ~550 ms per tick against the server's 250 ms ticks. See its README.
- **`clients/c64-tcp/`** - the same for the C64 with a FujiNet (cc65 + fujinet-lib, network device
  16), built with `../development-kit-c64-and-fujinet`. Converts PETSCII<->ASCII on the wire.
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

## Testing status (as of 2026-11-14)

- **Engine (all of `server/engine/` and `server/games/`):** exercised
  end-to-end with `bots/game_bot.sh` against a running `--headless`
  server - Tron parity, a full Snake match, per-game `STATS`/CSV
  separation, the compatibility-filtering scenario (WiC64 excluded from
  Snake, `ERR INCOMPATIBLE` on pairing, the original waiting player
  unaffected, a later compatible bot still pairs into it), and the
  bounded invalid-selection retry (5 retries then disconnect) all
  verified working as designed.
- **Clients - emulators only, none on real hardware yet:**
  - Atari BASIC (`clients/atari/`): atari800 + FujiNet-PC, a match won against the C64 (FIRE not
    checked separately).
  - Atari C (`clients/atari-tcp/`, build 3): atari800 + FujiNet-PC, played.
  - C64 C (`clients/c64-tcp/`, build 2): VICE + FujiNet-PC, matches incl. Artillery FIRE; the
    server prompt with an IP and a host name.
  - CPC C (`clients/cpc-tcp/`, build 2): CPCSyntaxError, matches; the server prompt incl. an
    unknown host.
  - Apple II C (`clients/apple2-tcp/`): FujiNet Go Apple II, a match won with build 1; build 2
    (server prompt) built but not run in the emulator.
- **New-game example** (`docs/howto-new-game.md`): logic test + two bots over the network.
- **Screenshots** of the games: `tools/game_screenshots.py` (real game + display code, scripted
  players).

## Development kits in the repo (2026-11-14)

- `devkits/` holds the four kits (Atari, C64, CPC, Apple II), merged with `git subtree --squash`
  from their own repos (`../development-kit-*`, GitHub joergr72idb/development-kit-*). Change a kit
  in its own repo, then `git subtree pull --prefix=devkits/<kit> ../<kit> main --squash` here.
- Only our own files: upstream tools are cloned and built by each `tools/setup.sh`, unmodified.
  Copyrighted things stay out (Atari ROMs in `rom/`, ProDOS, built `opt/`) via the kits' `.gitignore`.
- Client Makefiles: `DEV` = `devkits/<kit>` if its `opt/` exists, otherwise `../../../<kit>`.
- How-to: `docs/howto-devkits.md`; credits for every upstream project: README, "Thanks".

## Server entered at runtime (C clients build 2, 2026-11-14)

- The C clients (C64, CPC, Apple II; later the Atari C client too) ask for the server at the start: RETURN keeps the default
  from `make SERVER=`/`PORT=`, or type `HOST` or `HOST:PORT`. Asked again after a failed connection,
  and on **S** at "PRESS A KEY TO PLAY AGAIN". Not stored on disk (a reboot brings the default back).
- CPC: `SERVER_HOST` is a string now; `a.b.c.d` is parsed in the client, anything else goes through
  `m4_dns`. C64/Apple II: FujiNet resolves names itself.
- Tested: CPC (CPCSyntaxError: `localhost:6502`, unknown host -> asks again), C64 (VICE + FujiNet-PC:
  `127.0.0.1:6502` and `localhost:6502`). Apple II: built, not run in the emulator yet.
- Testing VICE by hand needs `DYLD_LIBRARY_PATH=opt/vice-bridge` (the kit's `make run` sets it),
  otherwise VICE has no FujiNet and every connection fails.

## Atari client in C (2026-11-14)

- **`clients/atari-tcp/`** - the Atari counterpart of the C clients (cc65 `-t atari` + fujinet-lib,
  the Atari kit), derived from the Apple II client: same protocol code, runtime server prompt, FIRE.
  Joystick via the hardware (`PORTA`/`TRIG0`, see below). Talks to the FujiNet over SIO directly - no DOS,
  no `N:` handler; the FujiNet boots `rgbtcp.xex` itself (`disk-images/atari/rgbtcp.xex`).
- The BASIC client `clients/atari/` stays.
- Played in atari800 + FujiNet-PC (build 3). Two Atari pitfalls found on the way:
  - **Buzzing:** SIO's "noisy I/O" plays every transfer, and the client polls constantly - `SOUNDR` = 0.
  - **Joystick dead in the match:** the OS skips its stage-2 VBI (which copies the stick to
    `STICK0`/`STRIG0`) while SIO is running (`CRITIC`), i.e. nearly always here - read `PORTA` and
    `TRIG0` directly.
- `make run` boots the XEX through the FujiNet (`fn_boot.py` mounts it in slot 1, CONFIG off), not
  `atari800 -run`: with the BASIC client's DOS + `N:` handler disk in D1: the program overwrote the
  handler and the keyboard was dead. `make config` restores CONFIG.
