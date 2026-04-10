from pathlib import Path

import xxhash

CHUNK_SIZE = 1024 * 1024


def hash_file(path: Path) -> str:
    h = xxhash.xxh3_64()
    with open(path, "rb") as f:
        while chunk := f.read(CHUNK_SIZE):
            h.update(chunk)
    return h.hexdigest()
