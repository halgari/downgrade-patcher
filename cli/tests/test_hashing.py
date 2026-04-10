from pathlib import Path
from downgrade_common.hashing import hash_file


def test_hash_file_returns_hex_string(tmp_path: Path):
    f = tmp_path / "test.bin"
    f.write_bytes(b"hello world")
    result = hash_file(f)
    assert isinstance(result, str)
    assert len(result) == 16  # xxhash3_64 produces 16 hex chars


def test_hash_file_deterministic(tmp_path: Path):
    f = tmp_path / "test.bin"
    f.write_bytes(b"hello world")
    assert hash_file(f) == hash_file(f)


def test_hash_file_different_content(tmp_path: Path):
    f1 = tmp_path / "a.bin"
    f2 = tmp_path / "b.bin"
    f1.write_bytes(b"hello")
    f2.write_bytes(b"world")
    assert hash_file(f1) != hash_file(f2)
