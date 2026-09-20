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
SERVER_BUILD = 1
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
# RESULTS LOG (CSV) - one shared file across all games, with a "game" column
# so per-game leaderboards can be derived from it without splitting files.
# =============================================================================
RESULTS_CSV_PATH = os.path.join(BASE_DIR, "game_results.csv")  # <-- EDIT
CSV_FIELDNAMES = ["timestamp", "game", "p1_platform", "p1_name", "p2_platform", "p2_name",
                   "result", "winner_platform", "winner_name"]
# =============================================================================

LOG_FILE_PATH = os.path.join(BASE_DIR, "game_battle_server.log")  # <-- EDIT

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

GREETING_IMAGE = os.path.join(BASE_DIR, "assets/logos/greetings/greeting.jpg")  # <-- EDIT
GREETING_TEXT = "THANKS A LOT!"  # <-- EDIT
GREETING_DURATION = 20
# =============================================================================

GRID_W = 50
GRID_H = 30
TICK_RATE = 4  # ticks/sec = 250ms/tick - see retro-tron-battle/CLAUDE.md for the
# per-platform HTTP-polling latency measurements that motivated this value.

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
