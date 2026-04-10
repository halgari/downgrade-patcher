import json
import pytest
from pathlib import Path


@pytest.fixture
def sample_games_json(tmp_path: Path) -> Path:
    data = {
        "games": [
            {
                "slug": "skyrim-se",
                "name": "Skyrim Special Edition",
                "steam_app_id": 489830,
                "depot_ids": [489833, 489834],
                "exe_path": "SkyrimSE.exe",
                "best_of_both_worlds": True,
                "nexus_domain": "skyrimspecialedition",
                "nexus_mod_id": 12345,
            },
            {
                "slug": "fallout4",
                "name": "Fallout 4",
                "steam_app_id": 377160,
                "depot_ids": [377162],
                "exe_path": "Fallout4.exe",
                "best_of_both_worlds": False,
                "nexus_domain": "fallout4",
                "nexus_mod_id": 67890,
            },
        ]
    }
    path = tmp_path / "games.json"
    path.write_text(json.dumps(data))
    return path
