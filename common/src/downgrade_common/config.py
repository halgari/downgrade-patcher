import json
from pathlib import Path

from pydantic import BaseModel


class GameConfig(BaseModel):
    slug: str
    name: str
    steam_app_id: int
    depot_ids: list[int]
    exe_path: str
    best_of_both_worlds: bool
    nexus_domain: str
    nexus_mod_id: int


class GamesFile(BaseModel):
    games: list[GameConfig]


def load_games(path: Path) -> list[GameConfig]:
    data = json.loads(path.read_text())
    return GamesFile.model_validate(data).games


def find_game(games: list[GameConfig], slug: str) -> GameConfig | None:
    for game in games:
        if game.slug == slug:
            return game
    return None
