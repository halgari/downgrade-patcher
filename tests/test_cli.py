import json
from pathlib import Path
from click.testing import CliRunner
from downgrade_patcher.cli import main


def test_ingest_creates_manifest_and_indexes(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    depot = tmp_path / "depot"
    depot.mkdir()
    (depot / "SkyrimSE.exe").write_bytes(b"exe-content-v1")
    (depot / "Data").mkdir()
    (depot / "Data" / "Skyrim.esm").write_bytes(b"esm-content-v1")

    runner = CliRunner()
    result = runner.invoke(main, [
        "--store-root", str(store_root),
        "--games-config", str(sample_games_json),
        "ingest",
        "--game", "skyrim-se",
        "--version", "1.5.97",
        "--depot-path", str(depot),
    ])

    assert result.exit_code == 0, result.output

    # Manifest was created
    manifest_path = store_root / "skyrim-se" / "manifests" / "1.5.97.json"
    assert manifest_path.exists()
    manifest = json.loads(manifest_path.read_text())
    assert manifest["game"] == "skyrim-se"
    assert manifest["version"] == "1.5.97"
    assert len(manifest["files"]) == 2

    # Manifest index was created/updated
    index_path = store_root / "skyrim-se" / "manifest-index.json"
    assert index_path.exists()
    index = json.loads(index_path.read_text())
    assert len(index["versions"]) == 1

    # Hash index was created/updated
    hash_index_path = store_root / "skyrim-se" / "hash-index.json"
    assert hash_index_path.exists()
    hash_index = json.loads(hash_index_path.read_text())
    assert len(hash_index) == 2  # exe + esm


def test_ingest_second_version_updates_indexes(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

    # Ingest v1
    depot_v1 = tmp_path / "depot_v1"
    depot_v1.mkdir()
    (depot_v1 / "SkyrimSE.exe").write_bytes(b"exe-v1")

    runner.invoke(main, [
        "--store-root", str(store_root),
        "--games-config", str(sample_games_json),
        "ingest", "--game", "skyrim-se", "--version", "1.5.97",
        "--depot-path", str(depot_v1),
    ])

    # Ingest v2
    depot_v2 = tmp_path / "depot_v2"
    depot_v2.mkdir()
    (depot_v2 / "SkyrimSE.exe").write_bytes(b"exe-v2")

    result = runner.invoke(main, [
        "--store-root", str(store_root),
        "--games-config", str(sample_games_json),
        "ingest", "--game", "skyrim-se", "--version", "1.6.1170",
        "--depot-path", str(depot_v2),
    ])

    assert result.exit_code == 0, result.output

    # Manifest index has both versions
    index = json.loads(
        (store_root / "skyrim-se" / "manifest-index.json").read_text()
    )
    assert len(index["versions"]) == 2

    # Hash index has entries from both versions
    hash_index = json.loads(
        (store_root / "skyrim-se" / "hash-index.json").read_text()
    )
    assert len(hash_index) == 2  # two different exe hashes


def test_list_versions(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

    for ver, content in [("1.5.97", b"v1"), ("1.6.1170", b"v2")]:
        depot = tmp_path / f"depot_{ver}"
        depot.mkdir()
        (depot / "SkyrimSE.exe").write_bytes(content)
        runner.invoke(main, [
            "--store-root", str(store_root),
            "--games-config", str(sample_games_json),
            "ingest", "--game", "skyrim-se", "--version", ver,
            "--depot-path", str(depot),
        ])

    result = runner.invoke(main, [
        "--store-root", str(store_root),
        "--games-config", str(sample_games_json),
        "list-versions", "--game", "skyrim-se",
    ])

    assert result.exit_code == 0
    assert "1.5.97" in result.output
    assert "1.6.1170" in result.output


def test_list_versions_empty(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

    result = runner.invoke(main, [
        "--store-root", str(store_root),
        "--games-config", str(sample_games_json),
        "list-versions", "--game", "skyrim-se",
    ])

    assert result.exit_code == 0
    assert "No versions" in result.output
