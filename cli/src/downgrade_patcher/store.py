import json
import shutil
from pathlib import Path


class VersionStore:
    def __init__(self, root: Path):
        self.root = root

    def version_dir(self, game_slug: str, version: str) -> Path:
        return self.root / game_slug / version

    def manifest_path(self, game_slug: str, version: str) -> Path:
        return self.root / game_slug / "manifests" / f"{version}.json"

    def manifest_index_path(self, game_slug: str) -> Path:
        return self.root / game_slug / "manifest-index.json"

    def hash_index_path(self, game_slug: str) -> Path:
        return self.root / game_slug / "hash-index.json"

    def list_versions(self, game_slug: str) -> list[str]:
        game_dir = self.root / game_slug
        if not game_dir.exists():
            return []
        skip = {"manifests"}
        return sorted(
            d.name
            for d in game_dir.iterdir()
            if d.is_dir() and d.name not in skip
        )

    def ingest(self, game_slug: str, version: str, depot_path: Path) -> Path:
        dest = self.version_dir(game_slug, version)
        if dest.exists():
            raise FileExistsError(f"Version {version} already exists for {game_slug}")
        shutil.copytree(depot_path, dest)
        return dest
