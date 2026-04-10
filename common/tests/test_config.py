import json
import pytest
from pathlib import Path
from downgrade_common.config import GameConfig, load_games, find_game

@pytest.fixture
def sample_games_json(tmp_path: Path) -> Path:
    data = {
        "games": [
            {
                "slug": "skyrim-se", "name": "Skyrim Special Edition",
                "steam_app_id": 489830, "depot_ids": [489833, 489834],
                "exe_path": "SkyrimSE.exe", "best_of_both_worlds": True,
                "nexus_domain": "skyrimspecialedition", "nexus_mod_id": 12345,
            },
            {
                "slug": "fallout4", "name": "Fallout 4",
                "steam_app_id": 377160, "depot_ids": [377162],
                "exe_path": "Fallout4.exe", "best_of_both_worlds": False,
                "nexus_domain": "fallout4", "nexus_mod_id": 67890,
            },
        ]
    }
    path = tmp_path / "games.json"
    path.write_text(json.dumps(data))
    return path

def test_load_games_returns_all_games(sample_games_json):
    games = load_games(sample_games_json)
    assert len(games) == 2
    assert games[0].slug == "skyrim-se"
    assert games[1].slug == "fallout4"

def test_game_config_fields(sample_games_json):
    games = load_games(sample_games_json)
    skyrim = games[0]
    assert skyrim.name == "Skyrim Special Edition"
    assert skyrim.steam_app_id == 489830
    assert skyrim.depot_ids == [489833, 489834]
    assert skyrim.exe_path == "SkyrimSE.exe"
    assert skyrim.best_of_both_worlds is True
    assert skyrim.nexus_domain == "skyrimspecialedition"
    assert skyrim.nexus_mod_id == 12345

def test_find_game_by_slug(sample_games_json):
    games = load_games(sample_games_json)
    game = find_game(games, "skyrim-se")
    assert game.slug == "skyrim-se"

def test_find_game_unknown_slug(sample_games_json):
    games = load_games(sample_games_json)
    game = find_game(games, "nonexistent")
    assert game is None
