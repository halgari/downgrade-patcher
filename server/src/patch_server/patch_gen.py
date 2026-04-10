import asyncio
import shutil
from pathlib import Path

from downgrade_common.cache import CacheLayout, PatchMeta
from downgrade_common.chunking import generate_chunked_patches


class PatchGenerator:
    def __init__(self, cache: CacheLayout):
        self._cache = cache
        self._locks: dict[tuple[str, str], asyncio.Lock] = {}

    def _get_lock(self, source_hash: str, target_hash: str) -> asyncio.Lock:
        key = (source_hash, target_hash)
        if key not in self._locks:
            self._locks[key] = asyncio.Lock()
        return self._locks[key]

    async def ensure_patches(
        self,
        source_path: Path,
        target_path: Path,
        game_slug: str,
        source_hash: str,
        target_hash: str,
    ) -> PatchMeta:
        cached = self._cache.read_meta(game_slug, source_hash, target_hash)
        if cached is not None:
            return cached

        lock = self._get_lock(source_hash, target_hash)
        async with lock:
            cached = self._cache.read_meta(game_slug, source_hash, target_hash)
            if cached is not None:
                return cached

            try:
                meta = await asyncio.to_thread(
                    generate_chunked_patches,
                    source_path=source_path,
                    target_path=target_path,
                    game_slug=game_slug,
                    source_hash=source_hash,
                    target_hash=target_hash,
                    cache=self._cache,
                )
                return meta
            except Exception:
                patch_dir = self._cache.patch_dir(game_slug, source_hash, target_hash)
                shutil.rmtree(patch_dir, ignore_errors=True)
                raise
