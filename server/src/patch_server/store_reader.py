import json
from pathlib import Path
from downgrade_common.config import GameConfig, load_games


class StoreReader:
    def __init__(self, store_root: Path, config_dir: Path):
        self._games = load_games(config_dir / "games.json")
        self._manifest_indexes: dict[str, dict] = {}
        self._manifests: dict[str, dict[str, dict]] = {}
        self._hash_indexes: dict[str, dict[str, list[str]]] = {}

        for game in self._games:
            slug = game.slug
            game_dir = store_root / slug

            mi_path = game_dir / "manifest-index.json"
            if mi_path.exists():
                self._manifest_indexes[slug] = json.loads(mi_path.read_text())

            hi_path = game_dir / "hash-index.json"
            if hi_path.exists():
                self._hash_indexes[slug] = json.loads(hi_path.read_text())

            manifests_dir = game_dir / "manifests"
            if manifests_dir.exists():
                self._manifests[slug] = {}
                for path in manifests_dir.glob("*.json"):
                    data = json.loads(path.read_text())
                    self._manifests[slug][data["version"]] = data

    def get_games(self) -> list[GameConfig]:
        return self._games

    def get_manifest_index(self, game_slug: str) -> dict | None:
        return self._manifest_indexes.get(game_slug)

    def get_manifest(self, game_slug: str, version: str) -> dict | None:
        game_manifests = self._manifests.get(game_slug)
        if game_manifests is None:
            return None
        return game_manifests.get(version)

    def resolve_hash(self, game_slug: str, file_hash: str) -> Path | None:
        game_index = self._hash_indexes.get(game_slug)
        if game_index is None:
            return None
        paths = game_index.get(file_hash)
        if not paths:
            return None
        for p in paths:
            path = Path(p)
            if path.exists():
                return path
        return None
