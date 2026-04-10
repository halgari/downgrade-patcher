import json
from pathlib import Path
from downgrade_patcher.manifest import generate_manifest, build_manifest_index
from downgrade_patcher.config import GameConfig


SKYRIM_CONFIG = GameConfig(
    slug="skyrim-se",
    name="Skyrim Special Edition",
    steam_app_id=489830,
    depot_ids=[489833],
    exe_path="SkyrimSE.exe",
    best_of_both_worlds=True,
    nexus_domain="skyrimspecialedition",
    nexus_mod_id=12345,
)


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


def test_build_manifest_index(tmp_path: Path):
    v1_dir = tmp_path / "skyrim-se" / "1.5.97"
    v1_dir.mkdir(parents=True)
    (v1_dir / "SkyrimSE.exe").write_bytes(b"exe-v1")

    v2_dir = tmp_path / "skyrim-se" / "1.6.1170"
    v2_dir.mkdir(parents=True)
    (v2_dir / "SkyrimSE.exe").write_bytes(b"exe-v2")

    manifests = {
        "1.5.97": generate_manifest("skyrim-se", "1.5.97", v1_dir),
        "1.6.1170": generate_manifest("skyrim-se", "1.6.1170", v2_dir),
    }

    index = build_manifest_index("skyrim-se", SKYRIM_CONFIG, manifests)

    assert index["game"] == "skyrim-se"
    assert len(index["versions"]) == 2
    version_set = set(index["versions"].values())
    assert version_set == {"1.5.97", "1.6.1170"}


def test_manifest_index_uses_exe_path_from_config(tmp_path: Path):
    v1_dir = tmp_path / "skyrim-se" / "1.5.97"
    v1_dir.mkdir(parents=True)
    (v1_dir / "SkyrimSE.exe").write_bytes(b"exe-v1")
    (v1_dir / "other.dll").write_bytes(b"dll-v1")

    manifests = {
        "1.5.97": generate_manifest("skyrim-se", "1.5.97", v1_dir),
    }

    index = build_manifest_index("skyrim-se", SKYRIM_CONFIG, manifests)

    assert len(index["versions"]) == 1
