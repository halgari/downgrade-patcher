import pytest


@pytest.mark.asyncio
async def test_get_manifest_index(client):
    resp = await client.get("/api/skyrim-se/manifest/index")
    assert resp.status_code == 200
    data = resp.json()
    assert "versions" in data
    assert data["versions"]["aaa111"] == "1.5.97"


@pytest.mark.asyncio
async def test_get_manifest_index_unknown_game(client):
    resp = await client.get("/api/nonexistent/manifest/index")
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_get_manifest(client):
    resp = await client.get("/api/skyrim-se/manifest/1.5.97")
    assert resp.status_code == 200
    data = resp.json()
    assert data["game"] == "skyrim-se"
    assert data["version"] == "1.5.97"
    assert len(data["files"]) == 1


@pytest.mark.asyncio
async def test_get_manifest_unknown_version(client):
    resp = await client.get("/api/skyrim-se/manifest/9.9.9")
    assert resp.status_code == 404
