import shutil
from pathlib import Path

import pyzstd

from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta

CHUNK_SIZE = 8 * 1024 * 1024  # 8MB

# Compression level for patch generation (3 = fast, decent ratio)
COMPRESSION_LEVEL = 3


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
    """Generate chunked patches using pyzstd with a pre-built ZstdDict.

    Loads the source file once, creates a ZstdDict from it, then compresses
    each target chunk using that dict. Each chunk file is written to the cache
    immediately so the server can serve it before all chunks are done.
    Meta.json is written last to signal completion.
    """
    # Load source file once into memory as dict
    source_data = source_path.read_bytes()
    zstd_dict = pyzstd.ZstdDict(source_data, is_raw=True)

    # Pre-build the CDict for reuse across all chunks (the key optimization)
    # This digests the source data into hash tables once
    option = {pyzstd.CParameter.compressionLevel: COMPRESSION_LEVEL,
              pyzstd.CParameter.enableLongDistanceMatching: 1,
              pyzstd.CParameter.windowLog: 27}  # 128MB window

    patch_dir = cache.patch_dir(game_slug, source_hash, target_hash)
    patch_dir.mkdir(parents=True, exist_ok=True)

    chunk_metas = []
    chunk_index = 0

    try:
        with open(target_path, "rb") as f:
            while True:
                target_chunk = f.read(CHUNK_SIZE)
                if not target_chunk:
                    break

                # Compress this chunk using the pre-built dict
                patch_data = pyzstd.compress(
                    target_chunk,
                    level_or_option=option,
                    zstd_dict=zstd_dict,
                )

                # Write chunk to cache immediately (serveable before meta exists)
                chunk_path = patch_dir / str(chunk_index)
                chunk_path.write_bytes(patch_data)

                chunk_metas.append(ChunkMeta(index=chunk_index, size=len(patch_data)))
                chunk_index += 1

        # Write meta last — signals that all chunks are complete
        meta = PatchMeta(total_chunks=len(chunk_metas), chunks=chunk_metas)
        cache.write_meta(game_slug, source_hash, target_hash, meta)
        return meta

    except Exception:
        shutil.rmtree(patch_dir, ignore_errors=True)
        raise
