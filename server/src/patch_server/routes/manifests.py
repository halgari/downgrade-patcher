from fastapi import APIRouter, Request, HTTPException

router = APIRouter()


@router.get("/api/{game}/manifest/index")
async def get_manifest_index(game: str, request: Request):
    store_reader = request.app.state.store_reader
    index = store_reader.get_manifest_index(game)
    if index is None:
        raise HTTPException(status_code=404, detail=f"Unknown game: {game}")
    return index


@router.get("/api/{game}/manifest/{version}")
async def get_manifest(game: str, version: str, request: Request):
    store_reader = request.app.state.store_reader
    manifest = store_reader.get_manifest(game, version)
    if manifest is None:
        raise HTTPException(status_code=404, detail=f"Manifest not found: {game} {version}")
    return manifest
