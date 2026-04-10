from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI

from downgrade_common.cache import CacheLayout
from patch_server.store_reader import StoreReader
from patch_server.patch_gen import PatchGenerator
from patch_server.routes.games import router as games_router
from patch_server.routes.manifests import router as manifests_router
from patch_server.routes.patches import router as patches_router


def create_app(
    store_root: Path,
    cache_root: Path,
    config_dir: Path,
) -> FastAPI:
    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.store_reader = StoreReader(store_root, config_dir)
        app.state.cache = CacheLayout(cache_root)
        app.state.patch_gen = PatchGenerator(app.state.cache)
        yield

    app = FastAPI(lifespan=lifespan)

    @app.get("/health")
    async def health():
        return {"status": "ok"}

    app.include_router(games_router)
    app.include_router(manifests_router)
    app.include_router(patches_router)
    return app
