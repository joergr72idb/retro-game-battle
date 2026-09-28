"""
Shared render state - the asyncio side (network) writes here under
render_lock; the pygame side (display/pygame_loop.py) reads a snapshot
~30 times a second under the same lock. Ported from
retro-tron-battle/server/tron_server.py's update_render/snapshot_render,
extended with `game_id` (which game is currently showing) and `game_extra`
(per-game render data, e.g. Snake's food - see games/base.py's
render_snapshot()).
"""

import threading

render_lock = threading.Lock()
render_state = {
    "phase": "waiting",          # waiting | versus | countdown | playing | ended
    "status": "Waiting for challengers to connect...",
    "game_id": "",
    "p1_name": "", "p1_plat": "",
    "p2_name": "", "p2_plat": "",
    "trail1": set(), "trail2": set(),
    "pos1": None, "pos2": None,
    "photo1": None, "photo2": None,
    "logo1": None, "logo2": None,
    "countdown": "",
    "versus_start": 0.0,         # time.monotonic() when the versus screen began
    "games_played": 0,
    "wins": {},
    "draws": 0,
    "game_extra": {},
}


def update_render(**kwargs):
    with render_lock:
        render_state.update(kwargs)


def snapshot_render():
    with render_lock:
        return {
            "phase": render_state["phase"],
            "status": render_state["status"],
            "game_id": render_state["game_id"],
            "p1_name": render_state["p1_name"], "p1_plat": render_state["p1_plat"],
            "p2_name": render_state["p2_name"], "p2_plat": render_state["p2_plat"],
            "trail1": set(render_state["trail1"]), "trail2": set(render_state["trail2"]),
            "pos1": render_state["pos1"], "pos2": render_state["pos2"],
            "photo1": render_state["photo1"], "photo2": render_state["photo2"],
            "logo1": render_state["logo1"], "logo2": render_state["logo2"],
            "countdown": render_state["countdown"],
            "versus_start": render_state["versus_start"],
            "games_played": render_state["games_played"],
            "wins": dict(render_state["wins"]),
            "draws": render_state["draws"],
            "game_extra": dict(render_state["game_extra"]),
        }
