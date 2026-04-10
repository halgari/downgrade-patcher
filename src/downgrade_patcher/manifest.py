from pathlib import Path

from downgrade_patcher.hashing import hash_file
from downgrade_patcher.config import GameConfig


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
