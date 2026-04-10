import json
from pathlib import Path
import pytest
from httpx import AsyncClient, ASGITransport
from asgi_lifespan import LifespanManager
from patch_server.app import create_app


@pytest.fixture
def store_and_config(tmp_path: Path) -> tuple[Path, Path, Path]:
    store_root = tmp_path / "store"
    cache_root = tmp_path / "cache"
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    games = {
        "games": [{
            "slug": "skyrim-se", "name": "Skyrim Special Edition",
            "steam_app_id": 489830, "depot_ids": [489833],
            "exe_path": "SkyrimSE.exe", "best_of_both_worlds": True,
            "nexus_domain": "skyrimspecialedition", "nexus_mod_id": 12345,
        }]
    }
    (config_dir / "games.json").write_text(json.dumps(games))

    v_dir = store_root / "skyrim-se" / "1.5.97"
    v_dir.mkdir(parents=True)
    (v_dir / "SkyrimSE.exe").write_bytes(b"exe-v1-content")

    manifests_dir = store_root / "skyrim-se" / "manifests"
    manifests_dir.mkdir(parents=True)
    manifest = {
        "game": "skyrim-se", "version": "1.5.97",
        "files": [{"path": "SkyrimSE.exe", "size": 14, "xxhash3": "aaa111"}],
    }
    (manifests_dir / "1.5.97.json").write_text(json.dumps(manifest))

    mi = {"game": "skyrim-se", "versions": {"aaa111": "1.5.97"}}
    (store_root / "skyrim-se" / "manifest-index.json").write_text(json.dumps(mi))

    hi = {"aaa111": [str(v_dir / "SkyrimSE.exe")]}
    (store_root / "skyrim-se" / "hash-index.json").write_text(json.dumps(hi))

    return store_root, cache_root, config_dir


@pytest.fixture
async def client(store_and_config) -> AsyncClient:
    store_root, cache_root, config_dir = store_and_config
    app = create_app(store_root=store_root, cache_root=cache_root, config_dir=config_dir)
    async with LifespanManager(app):
        transport = ASGITransport(app=app)
        async with AsyncClient(transport=transport, base_url="http://test") as c:
            yield c
