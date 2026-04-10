from pathlib import Path
from downgrade_common.chunking import split_target_into_chunks, CHUNK_SIZE


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


import json
from unittest.mock import patch as mock_patch, call
from downgrade_common.chunking import generate_chunked_patches
from downgrade_common.cache import CacheLayout


def test_generate_chunked_patches_calls_zstd_per_chunk(tmp_path: Path):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source-dictionary-content")
    target = tmp_path / "target.bin"
    target.write_bytes(b"A" * CHUNK_SIZE + b"B" * 100)

    cache = CacheLayout(tmp_path / "cache")

    with mock_patch("downgrade_common.chunking.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 0
        def side_effect(cmd, **kwargs):
            out_idx = cmd.index("-o") + 1
            Path(cmd[out_idx]).write_bytes(b"fake-patch")
            return mock_run.return_value
        mock_run.side_effect = side_effect

        meta = generate_chunked_patches(
            source_path=source,
            target_path=target,
            game_slug="skyrim-se",
            source_hash="aaa111",
            target_hash="bbb222",
            cache=cache,
        )

    assert meta.total_chunks == 2
    assert mock_run.call_count == 2
    for c in mock_run.call_args_list:
        cmd = c[0][0]
        assert "--patch-from" in cmd
        assert str(source) in cmd


def test_generate_chunked_patches_writes_cache(tmp_path: Path):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source-content")
    target = tmp_path / "target.bin"
    target.write_bytes(b"target-content")

    cache = CacheLayout(tmp_path / "cache")

    with mock_patch("downgrade_common.chunking.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 0
        def side_effect(cmd, **kwargs):
            out_idx = cmd.index("-o") + 1
            Path(cmd[out_idx]).write_bytes(b"fake-patch")
            return mock_run.return_value
        mock_run.side_effect = side_effect

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

    chunk_path = cache.chunk_path("skyrim-se", "aaa111", "bbb222", 0)
    assert chunk_path.exists()
