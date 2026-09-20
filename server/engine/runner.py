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


async def drain_moves(players):
    """Non-blockingly reads and applies any pending MOVE/BYE commands for
    each player - ported verbatim from the reference."""
    for p in players:
        try:
            data = await asyncio.wait_for(p.reader.read(256), timeout=0.01)
        except asyncio.TimeoutError:
            data = b""
        except (ConnectionResetError, BrokenPipeError):
            p.alive = False
            continue
        if data:
            p.inbuf.feed(data)
        elif data == b"" and isinstance(p.reader, asyncio.StreamReader) and p.reader.at_eof():
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
                    p.input_queue.append(d)
                    if d == config.OPPOSITE.get(p.direction):
                        log(f"[move] {p.platform}/{p.name}: {d} ignored (reverse of {p.direction})")
                    elif d != p.direction:
                        log(f"[move] {p.platform}/{p.name}: {p.direction} -> {d}")
                        p.direction = d
            elif line == "BYE":
                p.alive = False
            elif line == "FIRE":
                # Generic trigger for turn-based games (e.g. games/artillery.py).
                # Tron/Snake never send it and never read p.fired, so this is a
                # no-op for them.
                p.fired = True


async def run_game(p1, p2, game):
    game.init_round(p1, p2)

    await send(p1.writer, game.start_message(p1, p2, 1))
    await send(p2.writer, game.start_message(p1, p2, 2))
    log("=" * 60)
    log(f"[*] GAME START {datetime.now().isoformat(timespec='seconds')} - {game.id} - "
        f"{p1.platform}/{p1.name} vs {p2.platform}/{p2.name}")
    log("=" * 60)

    photo1, photo2 = await asyncio.gather(resolve_photo(p1), resolve_photo(p2))

    from engine.photo import logo_path_for  # local import avoids a module-load cycle with photo.py

    update_render(
        phase="countdown",
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

    for count in (3, 2, 1):
        update_render(countdown=str(count))
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            await drain_moves(players)
            if not (p1.alive and p2.alive):
                break
            await asyncio.sleep(0.05)

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
