import json
from pathlib import Path
import pytest
from downgrade_common.config import GameConfig
from patch_server.store_reader import StoreReader


@pytest.fixture
def populated_store(tmp_path: Path) -> tuple[Path, Path]:
    store_root = tmp_path / "store"
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    game_config = {
        "games": [{
            "slug": "skyrim-se", "name": "Skyrim Special Edition",
            "steam_app_id": 489830, "depot_ids": [489833],
            "exe_path": "SkyrimSE.exe", "best_of_both_worlds": True,
            "nexus_domain": "skyrimspecialedition", "nexus_mod_id": 12345,
        }]
    }
    (config_dir / "games.json").write_text(json.dumps(game_config))

    version_dir = store_root / "skyrim-se" / "1.5.97"
    version_dir.mkdir(parents=True)
    (version_dir / "SkyrimSE.exe").write_bytes(b"exe-content")

    manifests_dir = store_root / "skyrim-se" / "manifests"
    manifests_dir.mkdir(parents=True)
    manifest = {
        "game": "skyrim-se", "version": "1.5.97",
        "files": [{"path": "SkyrimSE.exe", "size": 11, "xxhash3": "abc123"}],
    }
    (manifests_dir / "1.5.97.json").write_text(json.dumps(manifest))

    index = {"game": "skyrim-se", "versions": {"abc123": "1.5.97"}}
    (store_root / "skyrim-se" / "manifest-index.json").write_text(json.dumps(index))

    hash_index = {"abc123": [str(version_dir / "SkyrimSE.exe")]}
    (store_root / "skyrim-se" / "hash-index.json").write_text(json.dumps(hash_index))

    return store_root, config_dir


def test_get_games(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    games = reader.get_games()
    assert len(games) == 1
    assert games[0].slug == "skyrim-se"


def test_get_manifest_index(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    index = reader.get_manifest_index("skyrim-se")
    assert index["versions"]["abc123"] == "1.5.97"


def test_get_manifest_index_unknown_game(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.get_manifest_index("nonexistent") is None


def test_get_manifest(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    manifest = reader.get_manifest("skyrim-se", "1.5.97")
    assert manifest["version"] == "1.5.97"
    assert len(manifest["files"]) == 1


def test_get_manifest_unknown_version(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.get_manifest("skyrim-se", "9.9.9") is None


def test_resolve_hash(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    path = reader.resolve_hash("skyrim-se", "abc123")
    assert path is not None
    assert path.exists()


def test_resolve_hash_unknown(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.resolve_hash("skyrim-se", "nonexistent") is None


def test_resolve_hash_wrong_game(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.resolve_hash("fallout4", "abc123") is None
