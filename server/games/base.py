"""
GameModule: the plugin interface every game implements. The engine
(engine/runner.py, engine/pairing.py, display/pygame_loop.py) only ever
talks to games through this interface - it never imports a specific game
module's internals.
"""

from typing import Protocol


class GameModule(Protocol):
    id: str                              # short wire id, e.g. "TRON", "SNAKE" - used in
                                          # WAIT GAMES / GAME <n> / START <id> / ERR INCOMPATIBLE
    label: str                           # single-token (no spaces) menu label, e.g. "SNAKE-DUEL"
    compatible_variants: set[str] | None  # None = no restriction; else an allowlist of variant ids
    # optional, default True: a MOVE opposite to the current direction is ignored (Tron can't
    # turn back into its own trail). Pong sets False - a paddle has to go back and forth.
    reverse_guard: bool

    def init_round(self, p1, p2) -> None:
        """Sets each player's x/y/direction/trail (and any game-level extra
        state, e.g. shared food) in place, for the start of a new match."""
        ...

    def tick(self, p1, p2) -> None:
        """One tick's worth of movement + collision resolution, mutating
        p1/p2 in place (x, y, alive, trail). No return value - the caller
        (engine/runner.py) reads p1.alive/p2.alive afterward."""
        ...

    def start_message(self, p1, p2, playernum: int) -> str:
        """Builds the exact START line for the player who is player number
        `playernum` (1 or 2)."""
        ...

    def render_snapshot(self, p1, p2) -> dict:
        """Extra render_state fields beyond the generic ones (trail1/trail2/
        pos1/pos2 are already handled by the engine) - e.g. {"food": (x, y)}.
        Return {} if there's nothing extra to show."""
        ...

    def draw_extra(self, screen, state, draw_ctx) -> None:
        """Optional: draw anything beyond the generic body/grid rendering
        (e.g. Snake's food). `draw_ctx` is a dict of drawing helpers/constants
        handed in by display/pygame_loop.py. No-op by default."""
        ...
