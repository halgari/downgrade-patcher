from pathlib import Path

from downgrade_common.hashing import hash_file
from downgrade_common.config import GameConfig


def generate_manifest(game_slug: str, version: str, version_dir: Path) -> dict:
    files = []
    for file_path in sorted(version_dir.rglob("*")):
        if not file_path.is_file():
            continue
        relative = file_path.relative_to(version_dir)
        files.append({
            "path": relative.as_posix(),
            "size": file_path.stat().st_size,
            "xxhash3": hash_file(file_path),
        })
    return {
        "game": game_slug,
        "version": version,
        "files": files,
    }


def build_hash_index(
    game_slug: str,
    manifests: dict[str, dict],
    store_root: Path,
) -> dict[str, list[str]]:
    index: dict[str, list[str]] = {}
    for version, manifest in manifests.items():
        version_dir = store_root / game_slug / version
        for file_entry in manifest["files"]:
            file_hash = file_entry["xxhash3"]
            abs_path = str(version_dir / file_entry["path"])
            if file_hash not in index:
                index[file_hash] = []
            if abs_path not in index[file_hash]:
                index[file_hash].append(abs_path)
    return index


def build_manifest_index(
    game_slug: str,
    game_config: GameConfig,
    manifests: dict[str, dict],
) -> dict:
    versions = {}
    for version, manifest in manifests.items():
        for file_entry in manifest["files"]:
            if file_entry["path"] == game_config.exe_path:
                versions[file_entry["xxhash3"]] = version
                break
    return {
        "game": game_slug,
        "versions": versions,
    }
