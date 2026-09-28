"""
Shared data model: Player, Stats (now one-per-game), LineBuffer, CSV
persistence. Ported near-verbatim from retro-tron-battle/server/tron_server.py
(Player class, Stats class, load_stats_from_csv/append_result_to_csv,
LineBuffer) - see that file for the original, single-game version.

Changes from the reference:
  - Player gains `variant` (compatibility id, defaults to platform - see
    pairing.py) and `game_id` (empty while still choosing a game).
  - Stats is tracked per game_id (STATS dict below) instead of one global
    instance, and the CSV gains a `game` column as its first field.
"""

import asyncio
import csv
import os
import sys
from dataclasses import dataclass, field
from datetime import datetime

import config


@dataclass
class Player:
    reader: object
    writer: object
    platform: str
    name: str
    x: int = 0
    y: int = 0
    direction: str = "R"
    variant: str = ""       # compatibility id - defaults to platform.upper() if not given
    game_id: str = ""       # filled in once a GAME <n> selection is accepted
    alive: bool = True
    fired: bool = False     # set by a "FIRE" command, consumed+reset by a turn-based game's tick()
    input_queue: object = field(default_factory=list)  # raw MOVE letters since the last tick,
    # UNFILTERED by the direction/reversal-guard logic below - for a game like Artillery
    # where U/D/L/R mean "adjust angle/power" rather than "steer", not "which way am I
    # currently heading" (Tron's reversal guard would otherwise block e.g. lowering the
    # angle right after raising it, since D is OPPOSITE of U)
    trail: object = field(default_factory=list)  # Tron: set of cells; Snake: ordered list, head last
    pin: str = ""
    photo_task: object = field(default=None, repr=False)
    game_state: object = field(default=None, repr=False)  # opaque per-round extra state (e.g. shared Snake food)
    inbuf: object = field(default_factory=lambda: LineBuffer(), repr=False)
    pending_moves: list = field(default_factory=list, repr=False)  # [(apply_at, direction), ...]
    # for config.ATARI_LATENCY_EQUALIZER, see engine/runner.py's drain_moves()
    eq_delay: float | None = None  # fixed per-match delay from the dynamic
    # equalizer (engine/runner.py's setup_equalizer()), None = static Atari rule


class Stats:
    """Tracks games played / wins per platform / draws - for ONE game."""

    def __init__(self):
        self.games_played = 0
        self.wins = {}  # platform -> win count
        self.draws = 0

    def record_win(self, platform: str):
        self.games_played += 1
        self.wins[platform] = self.wins.get(platform, 0) + 1

    def record_draw(self):
        self.games_played += 1
        self.draws += 1

    def summary_line(self) -> str:
        parts = [f"GAMES {self.games_played}"]
        for platform, count in sorted(self.wins.items()):
            parts.append(f"{platform} {count}")
        parts.append(f"DRAWS {self.draws}")
        return "STATS " + " ".join(parts)

    def print_console(self):
        print(f"  Total games played: {self.games_played}")
        for platform, count in sorted(self.wins.items(), key=lambda kv: -kv[1]):
            print(f"    {platform:10s} wins: {count}")
        print(f"    {'DRAWS':10s} : {self.draws}")


STATS: dict[str, Stats] = {}  # game_id -> Stats, populated by games/__init__.py at import time


def stats_for(game_id: str) -> Stats:
    if game_id not in STATS:
        STATS[game_id] = Stats()
    return STATS[game_id]


def load_stats_from_csv():
    """Reads RESULTS_CSV_PATH and rebuilds every game's score from it (via
    the normal record_win/record_draw methods). Missing file -> start at 0.
    Malformed rows are skipped, not fatal."""
    if not os.path.exists(config.RESULTS_CSV_PATH):
        print(f"[stats] no {config.RESULTS_CSV_PATH} found - starting at 0")
        return
    loaded = 0
    skipped = 0
    try:
        with open(config.RESULTS_CSV_PATH, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    game_id = row.get("game") or ""
                    s = stats_for(game_id)
                    if row.get("result") == "WIN":
                        s.record_win(row["winner_platform"])
                        loaded += 1
                    elif row.get("result") == "DRAW":
                        s.record_draw()
                        loaded += 1
                    else:
                        skipped += 1
                except Exception:
                    skipped += 1
    except Exception as e:
        print(f"[stats] couldn't read {config.RESULTS_CSV_PATH}, starting at 0: {e}")
        return
    msg = f"[stats] loaded {loaded} previous games from {config.RESULTS_CSV_PATH}"
    if skipped:
        msg += f" ({skipped} malformed row(s) skipped)"
    print(msg)
    for game_id, s in STATS.items():
        print(f"  -- {game_id} --")
        s.print_console()


def append_result_to_csv(game_id: str, p1: "Player", p2: "Player", result: str,
                          winner_platform: str = "", winner_name: str = ""):
    try:
        is_new = not os.path.exists(config.RESULTS_CSV_PATH)
        with open(config.RESULTS_CSV_PATH, "a", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=config.CSV_FIELDNAMES)
            if is_new:
                writer.writeheader()
            writer.writerow({
                "timestamp": datetime.now().isoformat(timespec="seconds"),
                "game": game_id,
                "p1_platform": p1.platform, "p1_name": p1.name,
                "p2_platform": p2.platform, "p2_name": p2.name,
                "result": result,
                "winner_platform": winner_platform,
                "winner_name": winner_name,
            })
    except Exception as e:
        print(f"[stats] couldn't write result to {config.RESULTS_CSV_PATH}: {e}")


def log(msg: str, color: str | None = None) -> None:
    """Like print(), additionally appends msg to LOG_FILE_PATH. color wraps
    the CONSOLE copy only (e.g. config.MOVE_LOG_COLOR) in an ANSI escape -
    skipped when stdout isn't a real terminal, and the log file always gets
    the plain text either way."""
    if color and sys.stdout.isatty():
        print(f"{color}{msg}{config.ANSI_RESET}")
    else:
        print(msg)
    try:
        with open(config.LOG_FILE_PATH, "a", encoding="utf-8") as f:
            f.write(msg + "\n")
    except OSError as e:
        print(f"[log] couldn't write to {config.LOG_FILE_PATH}: {e}")


class LineBuffer:
    """Collects raw bytes from a socket and extracts complete lines, tolerant
    of \\r, \\n or \\r\\n as terminators. Ported verbatim from the reference."""

    def __init__(self):
        self.buf = bytearray()

    def feed(self, data: bytes):
        self.buf.extend(data)

    def pop_line(self):
        for i, b in enumerate(self.buf):
            if b in (0x0D, 0x0A):
                line = bytes(self.buf[:i])
                rest = self.buf[i + 1:]
                if rest[:1] in (b"\r", b"\n") and rest[:1] != bytes([b]):
                    rest = rest[1:]
                self.buf = rest
                return line.decode("ascii", errors="ignore")
        return None


async def send(writer, line: str):
    try:
        writer.write((line + "\r\n").encode("ascii", errors="ignore"))
        await writer.drain()
    except (ConnectionResetError, BrokenPipeError):
        pass


async def read_line(reader, timeout: float | None = None, buf: "LineBuffer | None" = None):
    """Reads a single line, CR/LF-tolerant. Pass `buf` (a persistent
    LineBuffer, e.g. a Player's `inbuf`) when more reads may follow on the
    same connection - otherwise any bytes beyond the first line that
    arrived in the same chunk are silently discarded when this call's
    disposable buffer goes out of scope. The reference only ever called
    this once per connection (the initial HELLO), so it never needed this;
    the game-selection step (engine/pairing.py) reads repeatedly and must
    pass a persistent buffer."""
    lb = buf if buf is not None else LineBuffer()
    try:
        while True:
            line = lb.pop_line()
            if line is not None:
                return line
            if timeout:
                data = await asyncio.wait_for(reader.read(256), timeout=timeout)
            else:
                data = await reader.read(256)
            if not data:
                return None
            lb.feed(data)
    except (asyncio.TimeoutError, ConnectionResetError):
        return None


def display_name_for(name: str, pin: str) -> str:
    """If a real visitor PIN was given, show that instead of the client's
    fixed platform-stand-in name."""
    if pin and pin.upper() not in ("NONE", "-"):
        return pin
    return name


def variant_for(platform: str, variant: str) -> str:
    """Defaults an empty/placeholder variant to the platform itself."""
    if not variant or variant.upper() in ("-", "NONE"):
        return platform.upper()
    return variant.upper()
