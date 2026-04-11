import asyncio

from fastapi import APIRouter, Request, HTTPException
from fastapi.responses import FileResponse, Response

router = APIRouter()

CACHE_HEADERS = {"Cache-Control": "public, max-age=31536000, immutable"}


@router.get("/api/{game}/patch/{source_hash}/{target_hash}/meta")
async def get_patch_meta(game: str, source_hash: str, target_hash: str, request: Request):
    store_reader = request.app.state.store_reader
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

    chunk_path = cache.chunk_path(game, source_hash, target_hash, chunk_index)

    # If the chunk already exists on disk, serve it immediately
    if chunk_path.exists():
        return FileResponse(
            path=chunk_path,
            media_type="application/octet-stream",
            headers=CACHE_HEADERS,
        )

    # If meta exists, we know the total chunks — reject out of range immediately
    meta = cache.read_meta(game, source_hash, target_hash)
    if meta is not None:
        if chunk_index < 0 or chunk_index >= meta.total_chunks:
            raise HTTPException(status_code=404, detail="Chunk index out of range")
        # Meta exists but chunk file doesn't — something is wrong
        raise HTTPException(status_code=500, detail="Chunk file missing from cache")

    # Neither chunk nor meta exist — kick off generation and wait for this chunk
    asyncio.ensure_future(patch_gen.ensure_patches(
        source_path=source_path, target_path=target_path,
        game_slug=game, source_hash=source_hash, target_hash=target_hash,
    ))

    # Wait for up to 120 seconds for this specific chunk to appear
    for _ in range(240):
        await asyncio.sleep(0.5)
        if chunk_path.exists():
            return FileResponse(
                path=chunk_path,
                media_type="application/octet-stream",
                headers=CACHE_HEADERS,
            )
        # Check if meta appeared (generation finished) and chunk still missing
        meta = cache.read_meta(game, source_hash, target_hash)
        if meta is not None:
            if chunk_index >= meta.total_chunks:
                raise HTTPException(status_code=404, detail="Chunk index out of range")
            if not chunk_path.exists():
                raise HTTPException(status_code=500, detail="Chunk file missing from cache")

    raise HTTPException(status_code=504, detail="Chunk generation timed out")
