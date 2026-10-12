"""
Retro Game Battle - shared configuration.

Ported from retro-tron-battle/server/tron_server.py's top-of-file config
block. See that file (and its CLAUDE.md) for the hard-won reasoning behind
most of these values - not repeated here.
"""

import os

# =============================================================================
# VERSION - increment on every content change (shown in console + window)
# =============================================================================
SERVER_BUILD = 8
# =============================================================================

# =============================================================================
# REPO BASE DIRECTORY - all relative paths below are resolved from here, not
# the current working directory, so the server works unchanged no matter
# where it's started from (as long as server/ stays next to assets/).
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# =============================================================================

# =============================================================================
# DEMO SCROLL TEXT - RUNS ACROSS THE PLAYFIELD WHILE WAITING FOR PLAYERS
# =============================================================================
SCROLL_TEXT = " RETRO GAME BATTLE - PICK YOUR GAME "  # <-- EDIT: your own text
SCROLL_FONT_PATH = os.path.join(BASE_DIR, "assets/font/Flynn-4v54.ttf")  # <-- EDIT, or "" for default font
SCROLL_SPEED = 8
SCROLL_FONT_SIZE = 180
SCROLL_WAVE_AMPLITUDE = 120
SCROLL_WAVE_FREQ = 0.10
SCROLL_WAVE_SPEED = 0.12
SCROLL_COLOR_SPEED = 0.02
# =============================================================================

SCROLL_LOOPS_BEFORE_HIGHSCORE = 2
HIGHSCORE_DURATION = 20
# =============================================================================

# =============================================================================
# SOUND - CHIP-STYLE EFFECTS, GENERATED AT STARTUP (NO SAMPLE FILES)
# =============================================================================
# One-shot effects only (no background loops): versus slide + slam,
# countdown beeps + GO, crash, win/draw fanfare. Without a working audio
# device the server just runs silent.
SOUND_ENABLED = True     # <-- EDIT: False = no sound at all
SOUND_VOLUME = 0.6       # <-- EDIT: master volume, 0.0 - 1.0
# Attract music: an original chiptune (~36 s) that plays on the waiting
# screen, then MUSIC_GAP seconds of silence before it repeats (so it
# doesn't wear on everyone at the booth); fades out when a match starts.
MUSIC_ENABLED = True     # <-- EDIT: False = effects only, no music
MUSIC_VOLUME = 0.35      # <-- EDIT: 0.0 - 1.0, relative to the effects
MUSIC_GAP = 20           # <-- EDIT: seconds of silence between repeats
# =============================================================================

# =============================================================================
# VERSUS SCREEN - SHOWN RIGHT AFTER PAIRING, BEFORE THE 3-2-1 COUNTDOWN
# =============================================================================
# Both visitor photos (platform logo if there's no photo) slide in big from
# the left/right edges while pixelating into focus, a color-cycling "VS"
# slams in between them with a flash and a screen shake, copper-style
# raster bars roll behind, CRT scanlines on top - then the whole thing
# fades away and reveals the playfield for the countdown. Clients don't
# notice anything: they already got START and just keep polling, and
# direction changes are read in the whole time (like in the countdown).
VERSUS_SCREEN = True     # <-- EDIT: False = straight to the countdown, as before
VERSUS_DURATION = 4.0    # <-- EDIT: seconds in total, including the fade-out
VERSUS_FADE = 0.8        # <-- EDIT: seconds of that spent fading away
# =============================================================================


# =============================================================================
# RESULTS LOG (CSV) - one shared file across all games, with a "game" column
# so per-game leaderboards can be derived from it without splitting files.
# =============================================================================
RESULTS_CSV_PATH = os.path.join(BASE_DIR, "game_results.csv")  # <-- EDIT
CSV_FIELDNAMES = ["timestamp", "game", "p1_platform", "p1_name", "p2_platform", "p2_name",
                   "result", "winner_platform", "winner_name"]
# =============================================================================

LOG_FILE_PATH = os.path.join(BASE_DIR, "game_battle_server.log")  # <-- EDIT

# ANSI color for steering ([move]) lines on the console, e.g. for a second
# monitor showing this terminal next to the pygame playfield - makes
# steering easy to spot among the other log lines. Never written to
# LOG_FILE_PATH, just the console.
# <-- EDIT: None disables coloring; other options: ANSI_CYAN, ANSI_GREEN
ANSI_YELLOW = "\033[93m"
ANSI_CYAN = "\033[96m"
ANSI_GREEN = "\033[92m"
ANSI_RESET = "\033[0m"
MOVE_LOG_COLOR = ANSI_YELLOW

# =============================================================================
# EVENT PHOTO / LOGO CONFIGURATION
# =============================================================================
PHOTO_SOURCE = "HTTP"  # <-- EDIT: "FTP" or "HTTP"

FTP_HOST = "192.168.17.158"     # <-- EDIT
FTP_PORT = 21
FTP_USER = "anonymous"
FTP_PASS = "anonymous@"
FTP_REMOTE_DIR = ""

HTTP_PHOTO_BASE_URL = "http://fotofix.classic-computing.de"  # <-- EDIT
HTTP_PHOTO_FILENAME = "photo.jpg"

PHOTO_CACHE_DIR = os.path.join(BASE_DIR, "photo_cache")

LOGO_DIR = os.path.join(BASE_DIR, "assets/logos")  # <-- EDIT if logos live elsewhere
LOGO_NAME_CANDIDATES = {
    "ATARI": ["atari"],
    "C64": ["commodore", "c64"],
    "CPC": ["schneider", "cpc", "amstrad"],
    "APPLE2": ["apple2", "apple"],
}
LOGO_EXTENSIONS = [".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG", ".gif", ".GIF", ".bmp", ".BMP"]
# =============================================================================

# =============================================================================
# TRIBUTE SCREEN - credits the projects behind the WiFi interfaces
# =============================================================================
TRIBUTE_TITLE = "SPECIAL THANKS TO"
TRIBUTE_DURATION = 30
TRIBUTE_ENTRIES = [  # (image, credit text, url to display) - <-- EDIT for your own credits
    (os.path.join(BASE_DIR, "assets/logos/projects/fujinet.jpg"),
     "THE FUJINET PROJECT", "fujinet.online"),
    (os.path.join(BASE_DIR, "assets/logos/projects/meatloaf.png"),
     "JAMIE JOHNSTON (IDOLPX) - MEATLOAF", "github.com/idolpx/meatloaf"),
    (os.path.join(BASE_DIR, "assets/logos/projects/m4.png"),
     "DUKE - M4 INTERFACE", "github.com/M4Duke/m4hardware"),
    (os.path.join(BASE_DIR, "assets/logos/projects/wic64.png"),
     "THE WIC64 PROJECT", "wic64.net/web/"),
]
# =============================================================================

GRID_W = 50
GRID_H = 30
TICK_RATE = 4  # ticks/sec = 250ms/tick - see retro-tron-battle/CLAUDE.md for the
# per-platform HTTP-polling latency measurements that motivated this value.

# =============================================================================
# ATARI INPUT-LATENCY EQUALIZER - ported from retro-tron-battle (see its
# CLAUDE.md, "Atari input-latency equalizer"). Atari/FujiNet is the only
# client on a persistent raw TCP connection, so a MOVE reaches the server
# essentially instantly, while Meatloaf/CPC only get a fresh direction
# through every ~1-2 ticks (HTTP-poll round-trip ~300ms/~550ms). Every
# MOVE from an Atari player is held back by a random delay drawn from
# [ATARI_LATENCY_MIN, ATARI_LATENCY_MAX] before it's applied (both to
# direction and to input_queue), in every game. Server-side only, no client
# change. Starting values copied from the reference, not yet playtested.
# =============================================================================
ATARI_LATENCY_EQUALIZER = True  # <-- EDIT: False = off, Atari reacts instantly again
ATARI_LATENCY_MIN = 0.25        # <-- EDIT: seconds, minimum artificial delay per move
ATARI_LATENCY_MAX = 0.45        # <-- EDIT: seconds, maximum artificial delay per move
# =============================================================================

# =============================================================================
# DYNAMIC PAIRWISE LATENCY EQUALIZER - ported from retro-tron-battle (server
# build 18, see its CLAUDE.md). Replaces the static Atari rule above per
# match whenever both players can be measured: during the versus screen and
# countdown every HTTP client's /tick interval is timed, effective input
# delay = EQ_POLL_FACTOR x median poll (raw TCP: EQ_TCP_DELAY), and only the
# FASTER player is held back by the difference, capped at EQ_MAX_DELAY,
# fixed for the whole match (moves keep their order). Too few samples (or
# False here) -> the static Atari rule applies for that match. Logged per
# game as "[equalizer] ...", see engine/runner.py's setup_equalizer().
# =============================================================================
DYNAMIC_LATENCY_EQUALIZER = True  # <-- EDIT: False = static Atari rule above only
EQ_POLL_FACTOR = 0.75   # <-- EDIT: effective delay per second of poll interval
EQ_TCP_DELAY = 0.0      # <-- EDIT: seconds, effective delay of a raw TCP client
EQ_MAX_DELAY = 0.45     # <-- EDIT: seconds, never hold anyone back longer
EQ_MIN_DELAY = 0.02     # below this difference, nobody gets delayed
EQ_MIN_SAMPLES = 3      # /tick intervals needed for a usable measurement

# A waiting HTTP player that hasn't polled /tick for this long is treated
# as gone (reset, new RUN, WiFi lost) and dropped instead of being paired
# (engine/pairing.py, stale_waiting_reason()). All HTTP clients keep
# polling while they wait: CPC ~70 ms, Meatloaf ~340 ms, WiC64 ~1.3 s.
WAITING_HTTP_STALE_S = 5.0
# =============================================================================

CELL = 18
HUD_HEIGHT = 120
FOOTER_HEIGHT = 44
PANEL_W = 170
FIELD_X = PANEL_W
WINDOW_W = PANEL_W * 2 + GRID_W * CELL
WINDOW_H = HUD_HEIGHT + GRID_H * CELL + FOOTER_HEIGHT

PLATFORM_COLORS = {
    "ATARI": (108, 174, 216),
    "C64": (183, 173, 35),
    "CPC": (70, 200, 110),
    "APPLE2": (220, 110, 60),
}
DEFAULT_COLOR = (200, 200, 200)
BG_COLOR = (10, 10, 18)
GRID_LINE_COLOR = (30, 30, 42)
HUD_BG_COLOR = (18, 18, 28)
TEXT_COLOR = (230, 230, 230)
DIM_TEXT_COLOR = (140, 140, 150)

FOOD_COLOR = (255, 90, 70)  # neutral board-furniture color, deliberately not a PLATFORM_COLORS value

DIRS = {
    "U": (0, -1),
    "D": (0, 1),
    "L": (-1, 0),
    "R": (1, 0),
}
OPPOSITE = {"U": "D", "D": "U", "L": "R", "R": "L"}

# Compatibility/variant ids that always exist regardless of which games are
# registered - not strictly needed by the engine (variants are free-form
# strings), just documented here for reference:
#   ATARI, C64, C64-WIC64, CPC
