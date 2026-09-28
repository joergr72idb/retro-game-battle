"""
Generic match runner: countdown, tick loop, end-of-game plumbing. Ported
from retro-tron-battle/server/tron_server.py's run_game() and
drain_moves(), with the Tron-specific spawn/movement/collision/START-string
pieces replaced by calls into a GameModule (see games/base.py). The
win/draw rule (exactly one alive -> winner, both dead same tick -> draw)
stays here, unchanged - it's already generic to any "both can die per
tick" duel.
"""

import asyncio
import random
import statistics
import time
from datetime import datetime

import config
from engine.model import send, log, stats_for, append_result_to_csv
from engine.render import update_render


async def resolve_photo(p, timeout: float = 3.0):
    if p.photo_task is None:
        return None
    try:
        return await asyncio.wait_for(p.photo_task, timeout=timeout)
    except Exception:
        return None


def _apply_move(p, d: str):
    """Actually applies a MOVE: raw input_queue entry plus the reverse-guarded
    direction change (with logging) - split out of drain_moves() so
    ATARI_LATENCY_EQUALIZER can call it late, once a delayed Atari move
    comes due, instead of right when the MOVE line is read."""
    p.input_queue.append(d)
    if d == config.OPPOSITE.get(p.direction):
        log(f"[move] {p.platform}/{p.name}: {d} ignored (reverse of {p.direction})",
            color=config.MOVE_LOG_COLOR)
    elif d != p.direction:
        log(f"[move] {p.platform}/{p.name}: {p.direction} -> {d}", color=config.MOVE_LOG_COLOR)
        p.direction = d


async def drain_moves(players):
    """Non-blockingly reads and applies any pending MOVE/BYE commands for
    each player - ported from the reference, including its Atari
    input-latency equalizer (delayed moves are applied here once due)."""
    now = time.monotonic()
    for p in players:
        if p.pending_moves:
            due = [m for m in p.pending_moves if m[0] <= now]
            if due:
                due.sort(key=lambda m: m[0])
                p.pending_moves = [m for m in p.pending_moves if m[0] > now]
                for _, d in due:
                    _apply_move(p, d)

    for p in players:
        # Read EVERYTHING that's waiting, not just one chunk: an HTTP
        # "connection" hands out one queued line per read(), and a client
        # polling faster than TICK_RATE (CPC since client build 10, ~70 ms
        # per /tick) queues several MOVEs per tick - reading only one per
        # tick built an ever-growing backlog, so its moves arrived seconds
        # late or not at all before the game ended.
        gone = False
        for _ in range(64):
            try:
                data = await asyncio.wait_for(p.reader.read(256), timeout=0.01)
            except asyncio.TimeoutError:
                break
            except (ConnectionResetError, BrokenPipeError):
                gone = True
                break
            if data:
                p.inbuf.feed(data)
                continue
            if isinstance(p.reader, asyncio.StreamReader) and p.reader.at_eof():
                gone = True
            break
        if gone:
            p.alive = False
            continue
        while True:
            line = p.inbuf.pop_line()
            if line is None:
                break
            line = line.strip().upper()
            if line.startswith("MOVE"):
                tokens = line.split()
                if len(tokens) > 1 and tokens[1] in config.DIRS:
                    d = tokens[1]
                    if p.eq_delay is not None:
                        delay = p.eq_delay
                    elif config.ATARI_LATENCY_EQUALIZER and p.platform == "ATARI":
                        delay = random.uniform(config.ATARI_LATENCY_MIN, config.ATARI_LATENCY_MAX)
                    else:
                        delay = 0.0
                    if delay > 0 or p.pending_moves:
                        # never overtake a move that's still waiting
                        apply_at = time.monotonic() + delay
                        if p.pending_moves:
                            apply_at = max(apply_at, max(m[0] for m in p.pending_moves))
                        p.pending_moves.append((apply_at, d))
                    else:
                        _apply_move(p, d)
            elif line == "BYE":
                p.alive = False
            elif line == "FIRE":
                # Generic trigger for turn-based games (e.g. games/artillery.py).
                # Tron/Snake never send it and never read p.fired, so this is a
                # no-op for them.
                p.fired = True


def _tick_gaps(p):
    """/tick intervals of an HTTP player (engine/net_http.py's
    HTTPPlayerConn), None for raw TCP - duck-typed, since importing
    net_http here would be an import cycle via engine/pairing.py."""
    return getattr(p.reader, "tick_gaps", None)


def measured_input_delay(p):
    """Returns (effective input delay in s, median poll interval in s or
    None for raw TCP) for the dynamic equalizer, or None if an HTTP
    client hasn't polled often enough since GAME START to tell."""
    gaps = _tick_gaps(p)
    if gaps is None:
        return config.EQ_TCP_DELAY, None
    if len(gaps) < config.EQ_MIN_SAMPLES:
        return None
    poll = statistics.median(gaps)
    return config.EQ_POLL_FACTOR * poll, poll


def setup_equalizer(p1, p2):
    """Called once per match, right before play starts: measures both
    players and sets eq_delay on both (the faster one gets the difference,
    the other 0), or leaves both at None (= static Atari rule) if a
    measurement is missing. See config.DYNAMIC_LATENCY_EQUALIZER."""
    m1, m2 = measured_input_delay(p1), measured_input_delay(p2)

    def desc(p, m):
        if m is None:
            return f"{p.platform}/{p.name} not measured"
        eff, poll = m
        return (f"{p.platform}/{p.name} poll {poll * 1000:.0f} ms -> {eff * 1000:.0f} ms"
                if poll is not None else f"{p.platform}/{p.name} TCP -> {eff * 1000:.0f} ms")

    if m1 is None or m2 is None:
        fallback = (f"static rule for this match (Atari {config.ATARI_LATENCY_MIN}-"
                    f"{config.ATARI_LATENCY_MAX} s)"
                    if config.ATARI_LATENCY_EQUALIZER else "no equalizing this match")
        log(f"[equalizer] {desc(p1, m1)}, {desc(p2, m2)} - {fallback}")
        return

    diff = m1[0] - m2[0]
    delay = min(abs(diff), config.EQ_MAX_DELAY)
    if delay < config.EQ_MIN_DELAY:
        delay = 0.0
    faster = p2 if diff > 0 else p1
    for p in (p1, p2):
        p.eq_delay = delay if p is faster else 0.0
    what = f"{faster.platform}/{faster.name} +{delay * 1000:.0f} ms" if delay else "nobody delayed"
    log(f"[equalizer] {desc(p1, m1)}, {desc(p2, m2)} => {what}"
        + (" (capped)" if abs(diff) > config.EQ_MAX_DELAY else ""))


def log_ingame_polls(players, since):
    """Game-end diagnostic: median /tick interval per HTTP player during
    the actual match (samples after index since[p]), to compare with what
    the equalizer measured beforehand."""
    parts = []
    for p in players:
        gaps = _tick_gaps(p)
        if gaps is not None:
            gaps = gaps[since.get(id(p), 0):]
            if gaps:
                parts.append(f"{p.platform}/{p.name} {statistics.median(gaps) * 1000:.0f} ms "
                             f"({len(gaps)} polls)")
    if parts:
        log("[equalizer] in-game poll median: " + ", ".join(parts))


async def run_game(p1, p2, game):
    game.init_round(p1, p2)
    for p in (p1, p2):
        gaps = _tick_gaps(p)
        if gaps is not None:
            gaps.clear()  # measure from here, not the menu/wait

    await send(p1.writer, game.start_message(p1, p2, 1))
    await send(p2.writer, game.start_message(p1, p2, 2))
    log("=" * 60)
    log(f"[*] GAME START {datetime.now().isoformat(timespec='seconds')} - {game.id} - "
        f"{p1.platform}/{p1.name} vs {p2.platform}/{p2.name}")
    log("=" * 60)

    photo1, photo2 = await asyncio.gather(resolve_photo(p1), resolve_photo(p2))

    from engine.photo import logo_path_for  # local import avoids a module-load cycle with photo.py

    update_render(
        phase="versus" if config.VERSUS_SCREEN else "countdown",
        versus_start=time.monotonic(),
        status="",
        game_id=game.id,
        p1_name=p1.name, p1_plat=p1.platform,
        p2_name=p2.name, p2_plat=p2.platform,
        trail1=set(p1.trail), trail2=set(p2.trail),
        pos1=(p1.x, p1.y), pos2=(p2.x, p2.y),
        photo1=photo1, photo2=photo2,
        logo1=logo_path_for(p1.platform), logo2=logo_path_for(p2.platform),
        countdown="",
        game_extra=game.render_snapshot(p1, p2),
    )

    players = [p1, p2]

    async def pause_reading_moves(seconds):
        """Waits, but keeps reading direction changes (nothing gets lost)
        and stops early if someone sends BYE."""
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            await drain_moves(players)
            if not (p1.alive and p2.alive):
                break
            await asyncio.sleep(0.05)

    if config.VERSUS_SCREEN:
        await pause_reading_moves(config.VERSUS_DURATION)
        update_render(phase="countdown")

    for count in (3, 2, 1):
        update_render(countdown=str(count))
        await pause_reading_moves(1.0)

    if config.DYNAMIC_LATENCY_EQUALIZER:
        setup_equalizer(p1, p2)
    eq_since = {id(p): len(_tick_gaps(p)) for p in players if _tick_gaps(p) is not None}

    update_render(countdown="Go, get in there! Move!")
    await asyncio.sleep(0.6)
    update_render(phase="playing", countdown="")

    while p1.alive and p2.alive:
        tick_start = time.monotonic()

        await drain_moves(players)
        game.tick(p1, p2)

        update_render(trail1=set(p1.trail), trail2=set(p2.trail),
                      pos1=(p1.x, p1.y), pos2=(p2.x, p2.y),
                      game_extra=game.render_snapshot(p1, p2))

        elapsed = time.monotonic() - tick_start
        await asyncio.sleep(max(0.0, (1 / config.TICK_RATE) - elapsed))

    log_ingame_polls(players, eq_since)

    if p1.alive and not p2.alive:
        winner = p1
    elif p2.alive and not p1.alive:
        winner = p2
    else:
        winner = None

    stats = stats_for(game.id)
    if winner:
        stats.record_win(winner.platform)
        append_result_to_csv(game.id, p1, p2, "WIN", winner.platform, winner.name)
        result = f"END WIN {winner.platform} {winner.name}"
        status = f"{winner.platform}/{winner.name} WINS!"
        log(f"[=] GAME END {datetime.now().isoformat(timespec='seconds')} - "
            f"{winner.platform}/{winner.name} wins!")
    else:
        stats.record_draw()
        append_result_to_csv(game.id, p1, p2, "DRAW")
        result = "END DRAW"
        status = "DRAW - SIMULTANEOUS CRASH!"
        log(f"[=] GAME END {datetime.now().isoformat(timespec='seconds')} - draw (simultaneous crash)")

    update_render(phase="ended", status=status,
                  games_played=stats.games_played, wins=dict(stats.wins), draws=stats.draws)

    await send(p1.writer, result)
    await send(p2.writer, result)

    stat_line = stats.summary_line()
    await send(p1.writer, stat_line)
    await send(p2.writer, stat_line)
    stats.print_console()

    for p in (p1, p2):
        try:
            p.writer.close()
        except Exception:
            pass

    await asyncio.sleep(4)  # leave the result on screen for a few more seconds
    update_render(phase="waiting", status="Waiting for challengers to connect...",
                  trail1=set(), trail2=set(), pos1=None, pos2=None,
                  photo1=None, photo2=None, logo1=None, logo2=None, game_extra={})
