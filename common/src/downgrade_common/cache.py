import json
from pathlib import Path
from pydantic import BaseModel


class ChunkMeta(BaseModel):
    index: int
    size: int


class PatchMeta(BaseModel):
    total_chunks: int
    chunks: list[ChunkMeta]


class CacheLayout:
    def __init__(self, root: Path):
        self.root = root

    def patch_dir(self, game_slug: str, source_hash: str, target_hash: str) -> Path:
        return self.root / game_slug / source_hash / target_hash

    def meta_path(self, game_slug: str, source_hash: str, target_hash: str) -> Path:
        return self.patch_dir(game_slug, source_hash, target_hash) / "meta.json"

    def chunk_path(self, game_slug: str, source_hash: str, target_hash: str, index: int) -> Path:
        return self.patch_dir(game_slug, source_hash, target_hash) / str(index)

    def is_cached(self, game_slug: str, source_hash: str, target_hash: str) -> bool:
        return self.meta_path(game_slug, source_hash, target_hash).exists()

    def write_meta(self, game_slug: str, source_hash: str, target_hash: str, meta: PatchMeta) -> None:
        path = self.meta_path(game_slug, source_hash, target_hash)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(meta.model_dump_json(indent=2))

    def read_meta(self, game_slug: str, source_hash: str, target_hash: str) -> PatchMeta | None:
        path = self.meta_path(game_slug, source_hash, target_hash)
        if not path.exists():
            return None
        return PatchMeta.model_validate_json(path.read_text())
