from pathlib import Path
from downgrade_patcher.store import VersionStore


def test_version_dir(tmp_path: Path):
    store = VersionStore(tmp_path)
    result = store.version_dir("skyrim-se", "1.5.97")
    assert result == tmp_path / "skyrim-se" / "1.5.97"


def test_manifest_path(tmp_path: Path):
    store = VersionStore(tmp_path)
    result = store.manifest_path("skyrim-se", "1.5.97")
    assert result == tmp_path / "skyrim-se" / "manifests" / "1.5.97.json"


def test_manifest_index_path(tmp_path: Path):
    store = VersionStore(tmp_path)
    result = store.manifest_index_path("skyrim-se")
    assert result == tmp_path / "skyrim-se" / "manifest-index.json"


def test_hash_index_path(tmp_path: Path):
    store = VersionStore(tmp_path)
    result = store.hash_index_path("skyrim-se")
    assert result == tmp_path / "skyrim-se" / "hash-index.json"


def test_list_versions_empty(tmp_path: Path):
    store = VersionStore(tmp_path)
    assert store.list_versions("skyrim-se") == []


def test_ingest_copies_files(tmp_path: Path):
    store = VersionStore(tmp_path / "store")
    depot = tmp_path / "depot"
    depot.mkdir()
    (depot / "SkyrimSE.exe").write_bytes(b"exe-content")
    (depot / "Data").mkdir()
    (depot / "Data" / "Skyrim.esm").write_bytes(b"esm-content")

    store.ingest("skyrim-se", "1.5.97", depot)

    version_dir = store.version_dir("skyrim-se", "1.5.97")
    assert (version_dir / "SkyrimSE.exe").read_bytes() == b"exe-content"
    assert (version_dir / "Data" / "Skyrim.esm").read_bytes() == b"esm-content"


def test_ingest_appears_in_list_versions(tmp_path: Path):
    store = VersionStore(tmp_path / "store")
    depot = tmp_path / "depot"
    depot.mkdir()
    (depot / "game.exe").write_bytes(b"exe")

    store.ingest("skyrim-se", "1.5.97", depot)

    assert "1.5.97" in store.list_versions("skyrim-se")


def test_ingest_rejects_duplicate_version(tmp_path: Path):
    store = VersionStore(tmp_path / "store")
    depot = tmp_path / "depot"
    depot.mkdir()
    (depot / "game.exe").write_bytes(b"exe")

    store.ingest("skyrim-se", "1.5.97", depot)

    import pytest
    with pytest.raises(FileExistsError):
        store.ingest("skyrim-se", "1.5.97", depot)
