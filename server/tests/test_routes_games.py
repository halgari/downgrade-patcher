import pytest

@pytest.mark.asyncio
async def test_get_games(client):
    resp = await client.get("/api/games")
    assert resp.status_code == 200
    data = resp.json()
    assert len(data["games"]) == 1
    assert data["games"][0]["slug"] == "skyrim-se"
