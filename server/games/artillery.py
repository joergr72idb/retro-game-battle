"""
Artillery duel - new game module, turn-based (unlike every other game
here). Built specifically as the counterpart to Snake/Pong: those two
need continuous, low-latency steering and exclude the slow C64/WiC64
variant; this one only needs an occasional angle/power adjustment and a
single FIRE per turn, so WiC64's ~1280ms/poll (see
retro-tron-battle/CLAUDE.md) simply doesn't matter here - there's no
clock to race against between turns. compatible_variants is deliberately
None (no restriction), proving the architecture actually delivers what
the original design discussion asked for: "at the moment Tron is not
usable for a C64/WiC64 client, but an artillery game is usable."

Steering reuses the existing MOVE U/D/L/R wire command (no protocol
change) for angle/power adjustment - U/D = angle, L/R = power - but reads
it from Player.input_queue (raw, one entry per MOVE received) rather than
Player.direction, since Tron's reversal-guard on `direction` would
otherwise block e.g. lowering the angle right after raising it. FIRE is
the one genuinely new wire command in this project (see
engine/runner.py's drain_moves() and engine/net_http.py) - a trigger,
exactly like the "add the trigger button to the clients" idea from the
original design discussion.
"""

import math
from dataclasses import dataclass

import config

GRID_W = config.GRID_W
GRID_H = config.GRID_H
GROUND_Y = GRID_H - 2  # players stand here; a shot reaching this height without a hit is a miss

ANGLE_MIN, ANGLE_MAX, ANGLE_STEP = 10, 80, 5
POWER_MIN, POWER_MAX, POWER_STEP = 10, 100, 5
POWER_SCALE = 20.0   # power -> launch speed (cells/tick); tuned so a 45-degree,
# default-power (50) shot lands close to the opponent (distance ~= speed^2/GRAVITY,
# ~39 cells apart on the default GRID_W=50 grid) - the original 10.0 badly
# overshot the entire grid before the arc ever came back down (found while
# testing: a default shot flew ~170 cells before landing)
GRAVITY = 0.15        # added to vertical speed every tick
HIT_RADIUS = 1.5      # cells - a simplification: fast/high-power shots can tunnel past this


@dataclass
class ArtilleryRoundState:
    active: int                # 1 or 2 - whose turn it is
    projectile: dict | None    # {"x","y","vx","vy"} or None while waiting for FIRE


class ArtilleryGame:
    id = "ARTILLERY"
    label = "ARTILLERY-DUEL"
    compatible_variants = None  # no restriction - see module docstring

    def init_round(self, p1, p2):
        p1.x, p1.y, p1.direction = 5, GROUND_Y, "R"
        p2.x, p2.y, p2.direction = GRID_W - 6, GROUND_Y, "L"
        p1.alive = True
        p2.alive = True
        p1.angle, p1.power = 45, 50
        p2.angle, p2.power = 45, 50
        p1.fired = False
        p2.fired = False
        p1.input_queue = []
        p2.input_queue = []
        state = ArtilleryRoundState(active=1, projectile=None)
        p1.game_state = state
        p2.game_state = state
        self._sync_markers(p1, p2)

    def _sync_markers(self, p1, p2):
        # A single-cell marker per player, so the generic body renderer
        # (draw_body()) shows both duelists without needing custom code.
        p1.trail = {(p1.x, p1.y)}
        p2.trail = {(p2.x, p2.y)}

    def _apply_input(self, p):
        for d in p.input_queue:
            if d == "U":
                p.angle = min(ANGLE_MAX, p.angle + ANGLE_STEP)
            elif d == "D":
                p.angle = max(ANGLE_MIN, p.angle - ANGLE_STEP)
            elif d == "L":
                p.power = max(POWER_MIN, p.power - POWER_STEP)
            elif d == "R":
                p.power = min(POWER_MAX, p.power + POWER_STEP)
        p.input_queue.clear()

    def tick(self, p1, p2):
        self._apply_input(p1)
        self._apply_input(p2)

        state = p1.game_state  # shared ArtilleryRoundState, same object as p2.game_state
        active, other = (p1, p2) if state.active == 1 else (p2, p1)

        if state.projectile is None:
            if active.fired:
                angle_rad = math.radians(active.angle)
                speed = active.power / POWER_SCALE
                x_dir = 1 if active is p1 else -1
                state.projectile = {
                    "x": float(active.x), "y": float(active.y),
                    "vx": speed * math.cos(angle_rad) * x_dir,
                    "vy": -speed * math.sin(angle_rad),
                }
            active.fired = False
            other.fired = False  # a fire from whoever's NOT active is simply discarded
            return

        proj = state.projectile
        proj["vy"] += GRAVITY
        proj["x"] += proj["vx"]
        proj["y"] += proj["vy"]

        if abs(proj["x"] - other.x) <= HIT_RADIUS and abs(proj["y"] - other.y) <= HIT_RADIUS:
            other.alive = False  # active player wins - picked up by the generic runner.py logic
            state.projectile = None
            return

        if proj["y"] >= GROUND_Y or proj["x"] < 0 or proj["x"] >= GRID_W:
            state.projectile = None
            state.active = 2 if state.active == 1 else 1  # miss - pass the turn

    def start_message(self, p1, p2, playernum: int) -> str:
        return (f"START {self.id} {GRID_W} {GRID_H} "
                f"{p1.x} {p1.y} {p2.x} {p2.y} {playernum} {p1.platform} {p2.platform}")

    def render_snapshot(self, p1, p2) -> dict:
        state = p1.game_state
        return {
            "projectile": (state.projectile["x"], state.projectile["y"]) if state and state.projectile else None,
            "active": state.active if state else 1,
            "p1_angle": getattr(p1, "angle", 45), "p1_power": getattr(p1, "power", 50),
            "p2_angle": getattr(p2, "angle", 45), "p2_power": getattr(p2, "power", 50),
        }

    def draw_extra(self, screen, state, draw_ctx) -> None:
        pygame = draw_ctx["pygame"]
        field_x, hud_h, cell = draw_ctx["field_x"], draw_ctx["hud_height"], draw_ctx["cell"]
        extra = state.get("game_extra", {})

        ground_px = hud_h + GROUND_Y * cell + cell
        pygame.draw.line(screen, (90, 70, 50), (field_x, ground_px),
                          (field_x + GRID_W * cell, ground_px), 2)

        active = extra.get("active", 1)
        pos1, pos2 = state.get("pos1"), state.get("pos2")
        for pnum, pos in ((1, pos1), (2, pos2)):
            if pos is None or pnum != active:
                continue
            angle = extra.get(f"p{pnum}_angle", 45)
            x_dir = 1 if pnum == 1 else -1
            ax, ay = pos
            length = 2.5
            ex = ax + math.cos(math.radians(angle)) * length * x_dir
            ey = ay - math.sin(math.radians(angle)) * length
            sx = field_x + ax * cell + cell // 2
            sy = hud_h + ay * cell + cell // 2
            exs = field_x + ex * cell + cell // 2
            eys = hud_h + ey * cell + cell // 2
            pygame.draw.line(screen, (255, 220, 80), (sx, sy), (exs, eys), 2)

        proj = extra.get("projectile")
        if proj:
            px, py = proj
            cx = field_x + int(px * cell) + cell // 2
            cy = hud_h + int(py * cell) + cell // 2
            pygame.draw.circle(screen, (255, 140, 60), (cx, cy), max(3, cell // 3))
