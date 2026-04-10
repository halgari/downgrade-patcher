import asyncio
from pathlib import Path
from unittest.mock import patch as mock_patch
import pytest
from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta
from patch_server.patch_gen import PatchGenerator


@pytest.fixture
def cache(tmp_path: Path) -> CacheLayout:
    return CacheLayout(tmp_path / "cache")


@pytest.fixture
def generator(cache: CacheLayout) -> PatchGenerator:
    return PatchGenerator(cache)


@pytest.mark.asyncio
async def test_ensure_patches_generates_when_not_cached(tmp_path, cache, generator):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source-data")
    target = tmp_path / "target.bin"
    target.write_bytes(b"target-data")

    expected_meta = PatchMeta(
        total_chunks=1, chunks=[ChunkMeta(index=0, size=50)],
    )

    with mock_patch("patch_server.patch_gen.generate_chunked_patches") as mock_gen:
        mock_gen.return_value = expected_meta
        meta = await generator.ensure_patches(
            source_path=source, target_path=target,
            game_slug="skyrim-se", source_hash="aaa111", target_hash="bbb222",
        )

    assert meta.total_chunks == 1
    mock_gen.assert_called_once()


@pytest.mark.asyncio
async def test_ensure_patches_returns_cached(tmp_path, cache, generator):
    meta = PatchMeta(total_chunks=1, chunks=[ChunkMeta(index=0, size=50)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    with mock_patch("patch_server.patch_gen.generate_chunked_patches") as mock_gen:
        result = await generator.ensure_patches(
            source_path=tmp_path / "source.bin", target_path=tmp_path / "target.bin",
            game_slug="skyrim-se", source_hash="aaa111", target_hash="bbb222",
        )

    assert result.total_chunks == 1
    mock_gen.assert_not_called()


@pytest.mark.asyncio
async def test_ensure_patches_locks_concurrent_requests(tmp_path, cache, generator):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source")
    target = tmp_path / "target.bin"
    target.write_bytes(b"target")

    def mock_generate(**kwargs):
        meta = PatchMeta(total_chunks=1, chunks=[ChunkMeta(index=0, size=10)])
        kwargs["cache"].write_meta(
            kwargs["game_slug"], kwargs["source_hash"], kwargs["target_hash"], meta
        )
        return meta

    with mock_patch("patch_server.patch_gen.generate_chunked_patches") as mock_gen:
        mock_gen.side_effect = mock_generate

        results = await asyncio.gather(
            generator.ensure_patches(
                source_path=source, target_path=target,
                game_slug="skyrim-se", source_hash="aaa111", target_hash="bbb222",
            ),
            generator.ensure_patches(
                source_path=source, target_path=target,
                game_slug="skyrim-se", source_hash="aaa111", target_hash="bbb222",
            ),
        )

    assert mock_gen.call_count == 1
    assert all(r.total_chunks == 1 for r in results)
