import json
from pathlib import Path
from unittest.mock import patch as mock_patch
import pytest
from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta


@pytest.fixture
def store_with_two_versions(tmp_path: Path) -> tuple[Path, Path, Path]:
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

    v1_dir = store_root / "skyrim-se" / "1.5.97"
    v1_dir.mkdir(parents=True)
    (v1_dir / "SkyrimSE.exe").write_bytes(b"exe-v1-content")

    v2_dir = store_root / "skyrim-se" / "1.6.1170"
    v2_dir.mkdir(parents=True)
    (v2_dir / "SkyrimSE.exe").write_bytes(b"exe-v2-content-different")

    manifests_dir = store_root / "skyrim-se" / "manifests"
    manifests_dir.mkdir(parents=True)

    m1 = {"game": "skyrim-se", "version": "1.5.97",
          "files": [{"path": "SkyrimSE.exe", "size": 14, "xxhash3": "aaa111"}]}
    m2 = {"game": "skyrim-se", "version": "1.6.1170",
          "files": [{"path": "SkyrimSE.exe", "size": 24, "xxhash3": "bbb222"}]}
    (manifests_dir / "1.5.97.json").write_text(json.dumps(m1))
    (manifests_dir / "1.6.1170.json").write_text(json.dumps(m2))

    mi = {"game": "skyrim-se", "versions": {"aaa111": "1.5.97", "bbb222": "1.6.1170"}}
    (store_root / "skyrim-se" / "manifest-index.json").write_text(json.dumps(mi))

    hi = {"aaa111": [str(v1_dir / "SkyrimSE.exe")],
          "bbb222": [str(v2_dir / "SkyrimSE.exe")]}
    (store_root / "skyrim-se" / "hash-index.json").write_text(json.dumps(hi))

    return store_root, cache_root, config_dir


@pytest.fixture
async def patch_client(store_with_two_versions):
    from httpx import AsyncClient, ASGITransport
    from asgi_lifespan import LifespanManager
    from patch_server.app import create_app

    store_root, cache_root, config_dir = store_with_two_versions
    app = create_app(store_root=store_root, cache_root=cache_root, config_dir=config_dir)
    async with LifespanManager(app) as manager:
        transport = ASGITransport(app=manager.app)
        async with AsyncClient(transport=transport, base_url="http://test") as c:
            yield c, cache_root


@pytest.mark.asyncio
async def test_get_patch_meta_cached(patch_client):
    client, cache_root = patch_client
    cache = CacheLayout(cache_root)
    meta = PatchMeta(total_chunks=2, chunks=[ChunkMeta(index=0, size=100), ChunkMeta(index=1, size=50)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    resp = await client.get("/api/skyrim-se/patch/aaa111/bbb222/meta")
    assert resp.status_code == 200
    data = resp.json()
    assert data["total_chunks"] == 2


@pytest.mark.asyncio
async def test_get_patch_chunk_cached(patch_client):
    client, cache_root = patch_client
    cache = CacheLayout(cache_root)
    meta = PatchMeta(total_chunks=1, chunks=[ChunkMeta(index=0, size=10)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)
    cache.chunk_path("skyrim-se", "aaa111", "bbb222", 0).write_bytes(b"patch-data")

    resp = await client.get("/api/skyrim-se/patch/aaa111/bbb222/0")
    assert resp.status_code == 200
    assert resp.content == b"patch-data"
    assert resp.headers["cache-control"] == "public, max-age=31536000, immutable"


@pytest.mark.asyncio
async def test_get_patch_chunk_out_of_range(patch_client):
    client, cache_root = patch_client
    cache = CacheLayout(cache_root)
    meta = PatchMeta(total_chunks=1, chunks=[ChunkMeta(index=0, size=10)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    resp = await client.get("/api/skyrim-se/patch/aaa111/bbb222/5")
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_get_patch_unknown_hash(patch_client):
    client, _ = patch_client
    resp = await client.get("/api/skyrim-se/patch/nonexistent/bbb222/meta")
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_get_patch_cross_game_rejected(patch_client):
    client, _ = patch_client
    resp = await client.get("/api/fallout4/patch/aaa111/bbb222/meta")
    assert resp.status_code == 404
