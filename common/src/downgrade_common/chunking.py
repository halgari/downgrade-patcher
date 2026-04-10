import subprocess
import tempfile
from pathlib import Path

from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta

CHUNK_SIZE = 8 * 1024 * 1024  # 8MB


def split_target_into_chunks(target_path: Path, output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    chunk_paths = []
    chunk_index = 0
    with open(target_path, "rb") as f:
        while True:
            data = f.read(CHUNK_SIZE)
            if not data:
                break
            chunk_path = output_dir / str(chunk_index)
            chunk_path.write_bytes(data)
            chunk_paths.append(chunk_path)
            chunk_index += 1
    return chunk_paths


def generate_chunked_patches(
    source_path: Path,
    target_path: Path,
    game_slug: str,
    source_hash: str,
    target_hash: str,
    cache: CacheLayout,
) -> PatchMeta:
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)
        chunks_dir = tmp / "target_chunks"
        chunk_paths = split_target_into_chunks(target_path, chunks_dir)

        patch_dir = cache.patch_dir(game_slug, source_hash, target_hash)
        patch_dir.mkdir(parents=True, exist_ok=True)

        chunk_metas = []
        for i, target_chunk in enumerate(chunk_paths):
            patch_output = patch_dir / str(i)
            result = subprocess.run(
                [
                    "zstd",
                    "--patch-from", str(source_path),
                    str(target_chunk),
                    "-o", str(patch_output),
                ],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                import shutil
                shutil.rmtree(patch_dir, ignore_errors=True)
                raise RuntimeError(f"zstd failed for chunk {i}: {result.stderr}")
            chunk_metas.append(ChunkMeta(index=i, size=patch_output.stat().st_size))

        meta = PatchMeta(total_chunks=len(chunk_metas), chunks=chunk_metas)
        cache.write_meta(game_slug, source_hash, target_hash, meta)
        return meta
