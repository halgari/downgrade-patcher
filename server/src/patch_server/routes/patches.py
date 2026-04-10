from fastapi import APIRouter, Request, HTTPException
from fastapi.responses import FileResponse, Response

router = APIRouter()

CACHE_HEADERS = {"Cache-Control": "public, max-age=31536000, immutable"}


@router.get("/api/{game}/patch/{source_hash}/{target_hash}/meta")
async def get_patch_meta(game: str, source_hash: str, target_hash: str, request: Request):
    store_reader = request.app.state.store_reader
    cache = request.app.state.cache
    patch_gen = request.app.state.patch_gen

    source_path = store_reader.resolve_hash(game, source_hash)
    target_path = store_reader.resolve_hash(game, target_hash)
    if source_path is None or target_path is None:
        raise HTTPException(status_code=404, detail="Hash not found for this game")

    meta = await patch_gen.ensure_patches(
        source_path=source_path, target_path=target_path,
        game_slug=game, source_hash=source_hash, target_hash=target_hash,
    )
    return Response(
        content=meta.model_dump_json(),
        media_type="application/json",
        headers=CACHE_HEADERS,
    )


@router.get("/api/{game}/patch/{source_hash}/{target_hash}/{chunk_index}")
async def get_patch_chunk(
    game: str, source_hash: str, target_hash: str, chunk_index: int, request: Request,
):
    store_reader = request.app.state.store_reader
    cache = request.app.state.cache
    patch_gen = request.app.state.patch_gen

    source_path = store_reader.resolve_hash(game, source_hash)
    target_path = store_reader.resolve_hash(game, target_hash)
    if source_path is None or target_path is None:
        raise HTTPException(status_code=404, detail="Hash not found for this game")

    meta = await patch_gen.ensure_patches(
        source_path=source_path, target_path=target_path,
        game_slug=game, source_hash=source_hash, target_hash=target_hash,
    )

    if chunk_index < 0 or chunk_index >= meta.total_chunks:
        raise HTTPException(status_code=404, detail="Chunk index out of range")

    chunk_path = cache.chunk_path(game, source_hash, target_hash, chunk_index)
    if not chunk_path.exists():
        raise HTTPException(status_code=500, detail="Chunk file missing from cache")

    return FileResponse(
        path=chunk_path,
        media_type="application/octet-stream",
        headers=CACHE_HEADERS,
    )
