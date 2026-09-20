"""
GAME_REGISTRY: every playable game, in menu order. To add a new game:
write games/<name>.py implementing the GameModule interface (see base.py),
then register it below - nothing in engine/ or display/ needs to change.
"""

from games.tron import TronGame
from games.snake import SnakeGame

GAME_REGISTRY = {}


def _register(game):
    GAME_REGISTRY[game.id] = game


_register(TronGame())
_register(SnakeGame())

# Menu order = registration order above (Python dicts preserve insertion order).
GAME_ORDER = list(GAME_REGISTRY.keys())
