"""
Pong duel - new game module. Like Tron/Snake, a continuous real-time game
(paddles move every tick based on the last direction sent, same
persist-until-changed model as Tron's steering) - built specifically to
be a SECOND example (alongside Snake) of a game that genuinely needs
low latency and is therefore excluded from the slow C64/WiC64 variant,
same as Snake. First to WIN_SCORE points wins.

Only U/D move a paddle (L/R stop it: tick() never applies them, since
they're never checked below) - reuses the exact same MOVE wire command
Tron/Snake already use, no protocol change needed for this game.
"""

import random
from dataclasses import dataclass

import config

GRID_W = config.GRID_W
GRID_H = config.GRID_H

WIN_SCORE = 5
PADDLE_HALF = 2          # paddle occupies [y-PADDLE_HALF, y+PADDLE_HALF] -> 5 cells tall
PADDLE_SPEED = 1          # cells per tick while moving
BALL_SPEED_X = 1.0
BALL_SPEED_Y_MAX = 0.6


@dataclass
class PongRoundState:
    bx: float
    by: float
    bvx: float
    bvy: float


class PongGame:
    id = "PONG"
    label = "PONG"
    # Same reasoning as Snake: a continuously-updated paddle needs tighter
    # response than Tron's "just don't crash" - WiC64's ~1280ms/poll (see
    # retro-tron-battle/CLAUDE.md) makes real-time paddle control impossible.
    compatible_variants = {"ATARI", "C64", "CPC", "APPLE2"}
    # A paddle has to go back the way it came: D right after U must not be ignored as a
    # "reverse" the way Tron ignores it (engine/runner.py's _apply_move()).
    reverse_guard = False

    def init_round(self, p1, p2):
        p1.x, p1.y, p1.direction = 1, GRID_H // 2, "R"
        p2.x, p2.y, p2.direction = GRID_W - 2, GRID_H // 2, "L"
        p1.score = 0
        p2.score = 0
        p1.alive = True
        p2.alive = True
        state = self._serve(toward=random.choice([-1.0, 1.0]))
        p1.game_state = state
        p2.game_state = state
        self._sync_trails(p1, p2)

    def _serve(self, toward: float) -> PongRoundState:
        return PongRoundState(
            bx=GRID_W / 2.0, by=GRID_H / 2.0,
            bvx=BALL_SPEED_X * toward,
            bvy=random.uniform(-BALL_SPEED_Y_MAX, BALL_SPEED_Y_MAX),
        )

    def _sync_trails(self, p1, p2):
        # Reuses the generic body renderer (display/pygame_loop.py's
        # draw_body()) for the paddles - no game-specific paddle drawing
        # code needed, same trick Snake uses for its body.
        p1.trail = {(p1.x, y) for y in range(p1.y - PADDLE_HALF, p1.y + PADDLE_HALF + 1)}
        p2.trail = {(p2.x, y) for y in range(p2.y - PADDLE_HALF, p2.y + PADDLE_HALF + 1)}

    def tick(self, p1, p2):
        state = p1.game_state  # shared PongRoundState, same object as p2.game_state

        for p in (p1, p2):
            if p.direction == "U":
                p.y -= PADDLE_SPEED
            elif p.direction == "D":
                p.y += PADDLE_SPEED
            p.y = max(PADDLE_HALF, min(GRID_H - 1 - PADDLE_HALF, p.y))

        state.bx += state.bvx
        state.by += state.bvy

        if state.by <= 0:
            state.by = 0
            state.bvy = -state.bvy
        elif state.by >= GRID_H - 1:
            state.by = GRID_H - 1
            state.bvy = -state.bvy

        # Paddle bounce - only while the ball is heading toward that side.
        if state.bvx < 0 and state.bx <= p1.x + 1 and abs(state.by - p1.y) <= PADDLE_HALF:
            state.bvx = -state.bvx
            state.bvy += (state.by - p1.y) * 0.1
        elif state.bvx > 0 and state.bx >= p2.x - 1 and abs(state.by - p2.y) <= PADDLE_HALF:
            state.bvx = -state.bvx
            state.bvy += (state.by - p2.y) * 0.1

        scored = False
        if state.bx < 0:
            p2.score += 1
            scored = True
            if p2.score >= WIN_SCORE:
                p1.alive = False
            new_state = self._serve(toward=1.0)
        elif state.bx > GRID_W - 1:
            p1.score += 1
            scored = True
            if p1.score >= WIN_SCORE:
                p2.alive = False
            new_state = self._serve(toward=-1.0)

        if scored and p1.alive and p2.alive:
            p1.game_state = new_state
            p2.game_state = new_state

        self._sync_trails(p1, p2)

    def start_message(self, p1, p2, playernum: int) -> str:
        return (f"START {self.id} {GRID_W} {GRID_H} "
                f"{p1.x} {p1.y} {p2.x} {p2.y} {playernum} {p1.platform} {p2.platform}")

    def render_snapshot(self, p1, p2) -> dict:
        state = p1.game_state
        return {
            "ball": (state.bx, state.by) if state else None,
            "p1_score": getattr(p1, "score", 0),
            "p2_score": getattr(p2, "score", 0),
        }

    def draw_extra(self, screen, state, draw_ctx) -> None:
        pygame = draw_ctx["pygame"]
        extra = state.get("game_extra", {})
        ball = extra.get("ball")
        if ball:
            bx, by = ball
            cx = draw_ctx["field_x"] + int(bx * draw_ctx["cell"]) + draw_ctx["cell"] // 2
            cy = draw_ctx["hud_height"] + int(by * draw_ctx["cell"]) + draw_ctx["cell"] // 2
            pygame.draw.circle(screen, (255, 255, 255), (cx, cy), draw_ctx["cell"] // 2 - 1)

        # Score pips (small squares) along the top of the playfield, one
        # per point up to WIN_SCORE - avoids needing font access here.
        cell = draw_ctx["cell"]
        pip = max(4, cell // 2)
        top = draw_ctx["hud_height"] + 6
        for i in range(extra.get("p1_score", 0)):
            x = draw_ctx["field_x"] + 8 + i * (pip + 3)
            pygame.draw.rect(screen, (230, 230, 230), (x, top, pip, pip))
        for i in range(extra.get("p2_score", 0)):
            x = draw_ctx["field_x"] + config.GRID_W * cell - 8 - (i + 1) * (pip + 3)
            pygame.draw.rect(screen, (230, 230, 230), (x, top, pip, pip))
