from pathlib import Path

from downgrade_patcher.hashing import hash_file


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
