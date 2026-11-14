"""
Snake duel - new game module, not present in the reference repo. Reuses the
same grid/DIRS/tick-rate/"board never reaches the client" model as Tron
(games/tron.py), but with a fixed/bounded body length that only grows when
a player's head reaches a food cell, instead of Tron's ever-growing trail.

Body representation: Player.trail becomes an ORDERED list (head last),
unlike Tron's set - it has to shrink from the tail as the snake moves.
Food is board state, shared between both players via a small round-local
SnakeRoundState object referenced by both players' `game_state` (same
object, not a copy) - never sent on the wire, exactly like Tron's trails
never reach the client either.
"""

import random
from dataclasses import dataclass

import config

DIRS = config.DIRS
INITIAL_BODY_LEN = 3  # trailing cells behind the head at spawn (+1 for the head itself)


@dataclass
class SnakeRoundState:
    food: tuple


class SnakeGame:
    id = "SNAKE"
    label = "SNAKE-DUEL"
    # WiC64's ~1280ms/poll (see retro-tron-battle/CLAUDE.md) makes food-grabbing
    # largely luck-based, unlike Tron's "just don't crash" - excluded for now,
    # revisit if TICK_RATE or WiC64 speed changes.
    compatible_variants = {"ATARI", "C64", "CPC", "APPLE2"}

    def init_round(self, p1, p2):
        y = config.GRID_H // 2
        p1.x, p1.y, p1.direction = 5, y, "R"
        p2.x, p2.y, p2.direction = config.GRID_W - 6, y, "L"

        p1.trail = [(p1.x - i, y) for i in range(INITIAL_BODY_LEN, 0, -1)] + [(p1.x, y)]
        p2.trail = [(p2.x + i, y) for i in range(INITIAL_BODY_LEN, 0, -1)] + [(p2.x, y)]
        p1.target_len = len(p1.trail)
        p2.target_len = len(p2.trail)
        p1.alive = True
        p2.alive = True

        state = SnakeRoundState(food=self._spawn_food(p1.trail, p2.trail))
        p1.game_state = state
        p2.game_state = state

    def _spawn_food(self, body1, body2) -> tuple:
        occupied = set(body1) | set(body2)
        while True:
            fx = random.randrange(config.GRID_W)
            fy = random.randrange(config.GRID_H)
            if (fx, fy) not in occupied:
                return (fx, fy)

    def tick(self, p1, p2):
        state = p1.game_state  # shared SnakeRoundState, same object as p2.game_state

        dx1, dy1 = DIRS[p1.direction]
        dx2, dy2 = DIRS[p2.direction]
        new1 = (p1.x + dx1, p1.y + dy1)
        new2 = (p2.x + dx2, p2.y + dy2)

        eat1 = new1 == state.food
        eat2 = new2 == state.food

        # Exclude each body's about-to-be-dropped tail cell from the collision
        # check (unless this player eats this tick, so the tail isn't dropped)
        # - otherwise a snake could never legally step into the cell its own
        # tail is vacating this same tick.
        body1_check = set(p1.trail) if eat1 else set(p1.trail[1:])
        body2_check = set(p2.trail) if eat2 else set(p2.trail[1:])

        def in_bounds(pt):
            return 0 <= pt[0] < config.GRID_W and 0 <= pt[1] < config.GRID_H

        if not in_bounds(new1) or new1 in body1_check or new1 in body2_check:
            p1.alive = False
        if not in_bounds(new2) or new2 in body1_check or new2 in body2_check:
            p2.alive = False

        p1.x, p1.y = new1
        p2.x, p2.y = new2

        if p1.alive:
            p1.trail.append(new1)
            if eat1:
                p1.target_len += 1
            while len(p1.trail) > p1.target_len:
                p1.trail.pop(0)
        if p2.alive:
            p2.trail.append(new2)
            if eat2:
                p2.target_len += 1
            while len(p2.trail) > p2.target_len:
                p2.trail.pop(0)

        if (eat1 and p1.alive) or (eat2 and p2.alive):
            state.food = self._spawn_food(p1.trail, p2.trail)

    def start_message(self, p1, p2, playernum: int) -> str:
        return (f"START {self.id} {config.GRID_W} {config.GRID_H} "
                f"{p1.x} {p1.y} {p2.x} {p2.y} {playernum} {p1.platform} {p2.platform}")

    def render_snapshot(self, p1, p2) -> dict:
        state = p1.game_state
        return {"food": state.food if state else None}

    def draw_extra(self, screen, state, draw_ctx) -> None:
        food = state.get("game_extra", {}).get("food")
        if not food:
            return
        pygame = draw_ctx["pygame"]
        fx, fy = food
        cx = draw_ctx["field_x"] + fx * draw_ctx["cell"] + draw_ctx["cell"] // 2
        cy = draw_ctx["hud_height"] + fy * draw_ctx["cell"] + draw_ctx["cell"] // 2
        pygame.draw.circle(screen, config.FOOD_COLOR, (cx, cy), draw_ctx["cell"] // 2 - 1)
