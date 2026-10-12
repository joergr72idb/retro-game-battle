"""
Pairing + game selection. Extends retro-tron-battle's pair_and_maybe_start
(a single global waiting_player, immediate pairing) into a per-game waiting
queue with a game-selection step for whoever connects first. See
docs/protocol.md for the wire format this implements.

Core rule (settled in design discussion, see the plan): filtering is
symmetric and per-client - a client is only ever OFFERED games it's
compatible with. That means the only incompatibility that can still occur
is at pairing time, when a second/later client tries to join an
already-chosen game it isn't allowed to play - a one-directional check,
not a full matrix.
"""

import asyncio
import time

import config
import games as games_module
from engine.model import Player, LineBuffer, send, log, variant_for
from engine.photo import fetch_photo_blocking
from engine.render import update_render

MAX_SELECTION_RETRIES = 5

lock = asyncio.Lock()
waiting_players: dict[str, Player] = {}  # game_id -> the one player waiting for it


def stale_waiting_reason(w: Player, platform: str, name: str) -> str:
    """Says why waiting player w must not be paired with the newcomer (or
    "" if it's fine). Ported from retro-tron-battle server build 19: at
    CC2026 a client was paired with its own dead entry twice - the Atari
    after a FujiNet WiFi dropout (TCP socket never closed) and the C64
    after a restart (old HTTP session never polled again)."""
    if w.platform == platform and w.name == name:
        # Same machine joins again: the Atari sends nothing while waiting,
        # so this is the only way to notice a silently lost FujiNet
        # connection. (Two machines with the same default name and no PIN
        # would replace each other.)
        return "same client joined again"
    if w.reader.at_eof():
        return "connection closed"
    joined_at = getattr(w.reader, "joined_at", None)  # HTTPPlayerConn only -
    # duck-typed, importing net_http here would be an import cycle
    if joined_at is not None:
        idle = time.monotonic() - (w.reader.last_tick_at or joined_at)
        if idle > config.WAITING_HTTP_STALE_S:
            return f"no /tick for {idle:.1f} s"
    return ""


def drop_stale_waiting(platform: str, name: str):
    """Removes dead entries from waiting_players. Call under `lock`."""
    for gid, w in list(waiting_players.items()):
        reason = stale_waiting_reason(w, platform, name)
        if reason:
            del waiting_players[gid]
            log(f"[-] dropping waiting {w.platform}/{w.name} ({gid}): {reason}")
            try:
                w.writer.close()
            except Exception:
                pass


def compatible_games_for(variant: str):
    """Games (id, label) this variant is allowed to play, in menu order."""
    result = []
    for gid in games_module.GAME_ORDER:
        g = games_module.GAME_REGISTRY[gid]
        if g.compatible_variants is None or variant in g.compatible_variants:
            result.append((g.id, g.label))
    return result


def is_compatible(variant: str, game_id: str) -> bool:
    g = games_module.GAME_REGISTRY.get(game_id)
    if g is None:
        return False
    return g.compatible_variants is None or variant in g.compatible_variants


def format_game_list(prefix: str, entries) -> str:
    """e.g. format_game_list("WAIT", [("TRON","LIGHTCYCLE")]) ->
    "WAIT GAMES 1 TRON:LIGHTCYCLE" """
    parts = [prefix, "GAMES", str(len(entries))]
    parts += [f"{gid}:{label}" for gid, label in entries]
    return " ".join(parts)


async def _read_selection(player: Player):
    """Blocks until a `GAME <n>` line (or BYE/EOF) arrives on this player's
    connection - works identically for a real TCP socket and an
    HTTPPlayerConn, since both duck-type read()/at_eof(). Returns an int
    selection, or None on give-up/disconnect.

    Uses player.inbuf (a PERSISTENT LineBuffer, also the one drain_moves()
    reads from once the match starts) rather than a fresh buffer per call -
    this loop may run more than once per connection (retries), and a fresh
    buffer per call would silently discard any bytes beyond the first line
    that arrived in the same read (e.g. a fast client sending several
    selections back to back, or leftover bytes from parsing HELLO)."""
    while True:
        line = player.inbuf.pop_line()
        if line is None:
            data = await player.reader.read(256)
            if not data:
                return None
            player.inbuf.feed(data)
            continue
        line = line.strip().upper()
        if line == "BYE":
            return None
        if line.startswith("GAME"):
            rest = line[4:].strip()
            token = rest.split()[0] if rest.split() else ""
            if token.isdigit():
                return int(token)
        # anything else while still choosing (e.g. a stray MOVE) is ignored


async def join(platform: str, name: str, variant: str, pin: str,
                reader, writer, peer_desc: str = "", inbuf: LineBuffer | None = None):
    """Entry point used by both the TCP path (net_tcp.handle_client) and
    the HTTP bridge (net_http's /join) - mirrors the reference's
    pair_and_maybe_start, but the first connection now also has to pick a
    game before it can become the waiting player for that game. `inbuf`
    lets the caller hand over a buffer that may already hold bytes read
    past the HELLO line (see net_tcp.handle_client) - defaults to a fresh
    one for the HTTP path, which has no such leftover-bytes case."""
    from engine.runner import run_game  # local import: runner has no reverse dependency, but
    # keeping this import inside join() avoids relying on module-load order.

    variant = variant_for(platform, variant)
    log(f"[+] {platform}/{name} connected{(' from ' + peer_desc) if peer_desc else ''}"
        f" variant={variant}" + (f" (pin {pin})" if pin else ""))

    photo_task = asyncio.get_running_loop().run_in_executor(None, fetch_photo_blocking, pin)
    player = Player(reader, writer, platform, name, variant=variant, pin=pin, photo_task=photo_task,
                     inbuf=inbuf if inbuf is not None else LineBuffer())

    retries = 0
    while True:
        entries = compatible_games_for(player.variant)

        async with lock:
            drop_stale_waiting(platform, name)
            matched_game_id = None
            for gid in waiting_players:
                if is_compatible(player.variant, gid):
                    matched_game_id = gid
                    break

            if matched_game_id is not None:
                p1 = waiting_players.pop(matched_game_id)
                player.game_id = matched_game_id
                p1.game_id = matched_game_id
                asyncio.create_task(run_game(p1, player, games_module.GAME_REGISTRY[matched_game_id]))
                return

            if not entries:
                # nothing this client is allowed to play at all - tell it and stop
                await send(writer, format_game_list("ERR INCOMPATIBLE", entries))
                return

            if waiting_players:
                # someone's waiting, but for game(s) this client can't play
                await send(writer, format_game_list("ERR INCOMPATIBLE", entries))
            else:
                await send(writer, format_game_list("WAIT", entries))
            update_render(status=f"{platform}/{name} is choosing a game...")

        selection = await _read_selection(player)
        if selection is None:
            return  # gave up / disconnected while choosing

        if not (1 <= selection <= len(entries)):
            retries += 1
            if retries > MAX_SELECTION_RETRIES:
                await send(writer, "ERR TOO MANY INVALID SELECTIONS")
                try:
                    writer.close()
                except Exception:
                    pass
                return
            continue  # loop back - resends a (possibly updated) list

        chosen_game_id = entries[selection - 1][0]

        async with lock:
            drop_stale_waiting(platform, name)
            if chosen_game_id in waiting_players:
                p1 = waiting_players.pop(chosen_game_id)
                player.game_id = chosen_game_id
                p1.game_id = chosen_game_id
                asyncio.create_task(run_game(p1, player, games_module.GAME_REGISTRY[chosen_game_id]))
                return
            else:
                player.game_id = chosen_game_id
                waiting_players[chosen_game_id] = player
                await send(writer, "WAIT")
                update_render(status=f"{platform}/{name} is waiting for an opponent ({chosen_game_id})...")
                return
