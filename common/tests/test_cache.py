import json
from pathlib import Path
from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta


def test_patch_dir(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    result = layout.patch_dir("skyrim-se", "aaa111", "bbb222")
    assert result == tmp_path / "skyrim-se" / "aaa111" / "bbb222"


def test_meta_path(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    result = layout.meta_path("skyrim-se", "aaa111", "bbb222")
    assert result == tmp_path / "skyrim-se" / "aaa111" / "bbb222" / "meta.json"


def test_chunk_path(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    result = layout.chunk_path("skyrim-se", "aaa111", "bbb222", 3)
    assert result == tmp_path / "skyrim-se" / "aaa111" / "bbb222" / "3"


def test_patch_is_cached_false(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    assert layout.is_cached("skyrim-se", "aaa111", "bbb222") is False


def test_patch_is_cached_true(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    meta_path = layout.meta_path("skyrim-se", "aaa111", "bbb222")
    meta_path.parent.mkdir(parents=True)
    meta_path.write_text('{"total_chunks": 1, "chunks": [{"index": 0, "size": 100}]}')
    assert layout.is_cached("skyrim-se", "aaa111", "bbb222") is True


def test_write_and_read_meta(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    meta = PatchMeta(
        total_chunks=2,
        chunks=[ChunkMeta(index=0, size=8000), ChunkMeta(index=1, size=3000)],
    )
    layout.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    loaded = layout.read_meta("skyrim-se", "aaa111", "bbb222")
    assert loaded.total_chunks == 2
    assert loaded.chunks[0].size == 8000
    assert loaded.chunks[1].size == 3000


def test_read_meta_returns_none_when_missing(tmp_path: Path):
    layout = CacheLayout(tmp_path)
    assert layout.read_meta("skyrim-se", "aaa111", "bbb222") is None
