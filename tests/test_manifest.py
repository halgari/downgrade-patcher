import json
from pathlib import Path
from downgrade_patcher.manifest import generate_manifest


def test_generate_manifest_lists_all_files(tmp_path: Path):
    version_dir = tmp_path / "game"
    version_dir.mkdir()
    (version_dir / "game.exe").write_bytes(b"exe-bytes")
    (version_dir / "Data").mkdir()
    (version_dir / "Data" / "main.esm").write_bytes(b"esm-bytes")

    manifest = generate_manifest("skyrim-se", "1.5.97", version_dir)

    assert manifest["game"] == "skyrim-se"
    assert manifest["version"] == "1.5.97"
    paths = [f["path"] for f in manifest["files"]]
    assert "game.exe" in paths
    assert "Data/main.esm" in paths


def test_generate_manifest_includes_size_and_hash(tmp_path: Path):
    version_dir = tmp_path / "game"
    version_dir.mkdir()
    content = b"hello world"
    (version_dir / "game.exe").write_bytes(content)

    manifest = generate_manifest("skyrim-se", "1.5.97", version_dir)

    entry = manifest["files"][0]
    assert entry["size"] == len(content)
    assert isinstance(entry["xxhash3"], str)
    assert len(entry["xxhash3"]) == 16


def test_generate_manifest_uses_forward_slashes(tmp_path: Path):
    version_dir = tmp_path / "game"
    version_dir.mkdir()
    (version_dir / "Data").mkdir()
    (version_dir / "Data" / "Sub").mkdir()
    (version_dir / "Data" / "Sub" / "file.bsa").write_bytes(b"bsa")

    manifest = generate_manifest("skyrim-se", "1.5.97", version_dir)

    paths = [f["path"] for f in manifest["files"]]
    assert "Data/Sub/file.bsa" in paths
