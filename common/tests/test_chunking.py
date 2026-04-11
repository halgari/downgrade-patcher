from pathlib import Path

import pyzstd

from downgrade_common.chunking import split_target_into_chunks, generate_chunked_patches, CHUNK_SIZE
from downgrade_common.cache import CacheLayout


def test_split_small_file_single_chunk(tmp_path: Path):
    target = tmp_path / "target.bin"
    target.write_bytes(b"hello world")
    chunks_dir = tmp_path / "chunks"
    chunk_paths = split_target_into_chunks(target, chunks_dir)
    assert len(chunk_paths) == 1
    assert chunk_paths[0].read_bytes() == b"hello world"


def test_split_file_multiple_chunks(tmp_path: Path):
    target = tmp_path / "target.bin"
    data = b"A" * CHUNK_SIZE + b"B" * CHUNK_SIZE + b"C" * (CHUNK_SIZE // 2)
    target.write_bytes(data)
    chunks_dir = tmp_path / "chunks"
    chunk_paths = split_target_into_chunks(target, chunks_dir)
    assert len(chunk_paths) == 3
    assert chunk_paths[0].read_bytes() == b"A" * CHUNK_SIZE
    assert chunk_paths[1].read_bytes() == b"B" * CHUNK_SIZE
    assert chunk_paths[2].read_bytes() == b"C" * (CHUNK_SIZE // 2)


def test_split_creates_output_dir(tmp_path: Path):
    target = tmp_path / "target.bin"
    target.write_bytes(b"data")
    chunks_dir = tmp_path / "nested" / "chunks"
    chunk_paths = split_target_into_chunks(target, chunks_dir)
    assert chunks_dir.exists()
    assert len(chunk_paths) == 1


def test_generate_chunked_patches_produces_correct_chunks(tmp_path: Path):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source-dictionary-content-for-compression" * 100)
    target = tmp_path / "target.bin"
    # Two chunks worth of data
    target.write_bytes(b"A" * CHUNK_SIZE + b"B" * 100)

    cache = CacheLayout(tmp_path / "cache")

    meta = generate_chunked_patches(
        source_path=source,
        target_path=target,
        game_slug="skyrim-se",
        source_hash="aaa111",
        target_hash="bbb222",
        cache=cache,
    )

    assert meta.total_chunks == 2
    # Both chunk files should exist
    for i in range(2):
        chunk_path = cache.chunk_path("skyrim-se", "aaa111", "bbb222", i)
        assert chunk_path.exists()
        assert chunk_path.stat().st_size > 0


def test_generate_chunked_patches_writes_meta(tmp_path: Path):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source-content" * 50)
    target = tmp_path / "target.bin"
    target.write_bytes(b"target-content-different" * 50)

    cache = CacheLayout(tmp_path / "cache")

    meta = generate_chunked_patches(
        source_path=source,
        target_path=target,
        game_slug="skyrim-se",
        source_hash="aaa111",
        target_hash="bbb222",
        cache=cache,
    )

    loaded = cache.read_meta("skyrim-se", "aaa111", "bbb222")
    assert loaded is not None
    assert loaded.total_chunks == 1
    assert loaded.chunks[0].size == meta.chunks[0].size


def test_generate_chunked_patches_decompressible(tmp_path: Path):
    """Verify that generated patches can actually be decompressed with the source as dict."""
    source = tmp_path / "source.bin"
    source_data = b"shared-content-between-versions" * 200
    source.write_bytes(source_data)

    target = tmp_path / "target.bin"
    target_data = b"shared-content-between-versions" * 180 + b"new-stuff-in-target" * 20
    target.write_bytes(target_data)

    cache = CacheLayout(tmp_path / "cache")

    meta = generate_chunked_patches(
        source_path=source,
        target_path=target,
        game_slug="skyrim-se",
        source_hash="aaa111",
        target_hash="bbb222",
        cache=cache,
    )

    # Decompress each chunk and reassemble
    zstd_dict = pyzstd.ZstdDict(source_data, is_raw=True)
    reassembled = b""
    for i in range(meta.total_chunks):
        chunk_path = cache.chunk_path("skyrim-se", "aaa111", "bbb222", i)
        patch_data = chunk_path.read_bytes()
        decompressed = pyzstd.decompress(patch_data, zstd_dict=zstd_dict)
        reassembled += decompressed

    assert reassembled == target_data
