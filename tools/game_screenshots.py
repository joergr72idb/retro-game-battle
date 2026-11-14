#!/usr/bin/env python3
"""Screenshots of the four games for the README: the real game modules and the real display
(an invisible window, SDL's dummy driver), with two scripted players that can see the board.

Usage (from the repo root, with the project's Python and pygame):
    .venv/bin/python tools/game_screenshots.py [OUTDIR]

Writes OUTDIR/{tron,snake,pong,artillery}.png (default: docs/screenshots/games). Statistics and
the log of this run are discarded (os.devnull), never written to game_results.csv /
game_battle_server.log.
"""
import os
import random
import sys
import threading
import time
from collections import deque

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER = os.path.join(REPO, "server")
OUTDIR = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "docs/screenshots/games"))
os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"
sys.path.insert(0, SERVER)
os.chdir(SERVER)

import config  # noqa: E402
config.RESULTS_CSV_PATH = os.devnull     # no statistics or log from scripted matches
config.LOG_FILE_PATH = os.devnull

import pygame  # noqa: E402
import games  # noqa: E402
from engine.model import Player  # noqa: E402
from engine.render import update_render  # noqa: E402
from engine.photo import logo_path_for  # noqa: E402

random.seed(7)
os.makedirs(OUTDIR, exist_ok=True)
W, H, DIRS, OPP = config.GRID_W, config.GRID_H, config.DIRS, config.OPPOSITE

want_shot = {"name": None}
_flip = pygame.display.flip


def flip():
    _flip()
    if want_shot["name"]:
        pygame.image.save(pygame.display.get_surface(),
                          os.path.join(OUTDIR, want_shot["name"] + ".png"))
        want_shot["name"] = None


pygame.display.flip = flip


def shoot(name):
    want_shot["name"] = name
    while want_shot["name"]:
        time.sleep(0.02)


def free_area(start, blocked, limit=400):
    """cells reachable from start (flood fill, capped)"""
    if start in blocked or not (0 <= start[0] < W and 0 <= start[1] < H):
        return 0
    seen, q = {start}, deque([start])
    while q and len(seen) < limit:
        x, y = q.popleft()
        for dx, dy in DIRS.values():
            n = (x + dx, y + dy)
            if n not in seen and n not in blocked and 0 <= n[0] < W and 0 <= n[1] < H:
                seen.add(n)
                q.append(n)
    return len(seen)


def steer_safe(p, blocked, target=None, wiggle=0.15):
    """the direction with the most room; towards target if given; sometimes a turn for looks"""
    best = None
    for d, (dx, dy) in DIRS.items():
        if d == OPP.get(p.direction):
            continue
        n = (p.x + dx, p.y + dy)
        room = free_area(n, blocked)
        dist = abs(n[0] - target[0]) + abs(n[1] - target[1]) if target else 0
        score = room * 10 - dist * 3 + (0 if d == p.direction else -4) + random.random() * 30 * wiggle
        if best is None or score > best[0]:
            best = (score, d)
    if best:
        p.direction = best[1]


def start(game, plat1, name1, plat2, name2):
    p1, p2 = Player(None, None, plat1, name1), Player(None, None, plat2, name2)
    game.init_round(p1, p2)
    update_render(phase="playing", status="", game_id=game.id, countdown="",
                  p1_name=name1, p1_plat=plat1, p2_name=name2, p2_plat=plat2,
                  logo1=logo_path_for(plat1), logo2=logo_path_for(plat2), photo1=None, photo2=None)
    return p1, p2


def show(game, p1, p2):
    update_render(trail1=set(p1.trail), trail2=set(p2.trail), pos1=(p1.x, p1.y), pos2=(p2.x, p2.y),
                  game_extra=game.render_snapshot(p1, p2))
    time.sleep(0.04)


def tron():
    g = games.GAME_REGISTRY["TRON"]
    p1, p2 = start(g, "ATARI", "ATARIXL", "C64", "C64FUJINET")
    for t in range(170):
        blocked = set(p1.trail) | set(p2.trail)
        steer_safe(p1, blocked)
        steer_safe(p2, blocked)
        g.tick(p1, p2)
        show(g, p1, p2)
        if not (p1.alive and p2.alive):
            return False
    shoot("tron")
    return True


def snake():
    g = games.GAME_REGISTRY["SNAKE"]
    p1, p2 = start(g, "CPC", "SCHNEIDERCPC", "APPLE2", "APPLE2")
    for t in range(260):
        food = p1.game_state.food
        blocked = set(p1.trail[1:]) | set(p2.trail[1:])
        steer_safe(p1, blocked, food, 0.05)
        steer_safe(p2, blocked, food, 0.05)
        g.tick(p1, p2)
        show(g, p1, p2)
        if not (p1.alive and p2.alive):
            return False
    shoot("snake")
    return True


def pong():
    g = games.GAME_REGISTRY["PONG"]
    p1, p2 = start(g, "C64", "C64FUJINET", "CPC", "SCHNEIDERCPC")
    for t in range(2000):
        s = p1.game_state
        for p, lag in ((p1, 0.25), (p2, 0.35)):        # imperfect players, so points happen
            if random.random() < lag:
                continue
            p.direction = "U" if s.by < p.y - 0.5 else "D" if s.by > p.y + 0.5 else "L"
        g.tick(p1, p2)
        show(g, p1, p2)
        s = p1.game_state
        if p1.score + p2.score >= 3 and 0.35 < s.bx / W < 0.6 and p1.alive and p2.alive:
            shoot("pong")
            return True
        if not (p1.alive and p2.alive):
            return False
    return False


def artillery():
    g = games.GAME_REGISTRY["ARTILLERY"]
    p1, p2 = start(g, "APPLE2", "APPLE2", "ATARI", "ATARIXL")
    shots = 0
    for t in range(3000):
        s = p1.game_state
        active = p1 if s.active == 1 else p2
        if s.projectile is None:
            # turn the barrel a bit, then fire
            active.input_queue += random.choice([["U"], ["D"], ["R"], ["L"], []])
            if random.random() < 0.15:
                active.fired = True
                shots += 1
        g.tick(p1, p2)
        show(g, p1, p2)
        s = p1.game_state
        if shots >= 3 and s.projectile and 0.4 < s.projectile["x"] / W < 0.6 and s.projectile["y"] < H * 0.5:
            shoot("artillery")
            return True
        if not (p1.alive and p2.alive):
            p1, p2 = start(g, "APPLE2", "APPLE2", "ATARI", "ATARIXL")
            shots = 0
    return False


def run():
    time.sleep(2)                                          # let the display start
    for f in (tron, snake, pong, artillery):
        for attempt in range(20):
            if f():
                print(f.__name__, "ok after", attempt + 1, "attempt(s)", flush=True)
                break
        else:
            print(f.__name__, "FAILED", flush=True)
    print("written to", OUTDIR, flush=True)
    os._exit(0)


threading.Thread(target=run, daemon=True).start()
from display.pygame_loop import pygame_loop  # noqa: E402
pygame_loop()
