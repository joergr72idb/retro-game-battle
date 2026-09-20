"""
Tron/lightcycle duel - ported rule-for-rule from
retro-tron-battle/server/tron_server.py's run_game() (the movement/collision
block at lines 973-987 there, plus the spawn constants from
pair_and_maybe_start). Should behave identically to the reference: same
grid, same spawn points/directions, same collision rule, same win/draw logic.
"""

import config

DIRS = config.DIRS


class TronGame:
    id = "TRON"
    label = "LIGHTCYCLE"
    compatible_variants = None  # no restriction - already proven fine even at WiC64's ~1280ms/poll

    def init_round(self, p1, p2):
        p1.x, p1.y, p1.direction = 5, config.GRID_H // 2, "R"
        p2.x, p2.y, p2.direction = config.GRID_W - 6, config.GRID_H // 2, "L"
        p1.trail = {(p1.x, p1.y)}
        p2.trail = {(p2.x, p2.y)}
        p1.alive = True
        p2.alive = True

    def tick(self, p1, p2):
        players = (p1, p2)
        for p in players:
            dx, dy = DIRS[p.direction]
            p.x += dx
            p.y += dy

        for p in players:
            if not (0 <= p.x < config.GRID_W and 0 <= p.y < config.GRID_H):
                p.alive = False
            elif (p.x, p.y) in p1.trail or (p.x, p.y) in p2.trail:
                p.alive = False

        if p1.alive:
            p1.trail.add((p1.x, p1.y))
        if p2.alive:
            p2.trail.add((p2.x, p2.y))

    def start_message(self, p1, p2, playernum: int) -> str:
        return (f"START {self.id} {config.GRID_W} {config.GRID_H} "
                f"{p1.x} {p1.y} {p2.x} {p2.y} {playernum} {p1.platform} {p2.platform}")

    def render_snapshot(self, p1, p2) -> dict:
        return {}

    def draw_extra(self, screen, state, draw_ctx) -> None:
        pass
