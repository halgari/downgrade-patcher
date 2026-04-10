from fastapi import APIRouter, Request

router = APIRouter()

@router.get("/api/games")
async def get_games(request: Request):
    store_reader = request.app.state.store_reader
    games = store_reader.get_games()
    return {"games": [g.model_dump() for g in games]}
