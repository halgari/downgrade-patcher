# CLI Tooling + Version Store + Manifest Generation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Python CLI tool (`downgrade-tool`) that manages game versions on the server — downloading depots, ingesting files into the version store, generating manifests, and building indexes.

**Architecture:** A Python package (`downgrade_patcher`) with a Click CLI entry point. Each domain concern (config, hashing, store, manifest, depot) lives in its own focused module. The version store is a filesystem layout under a configurable root directory. All metadata (manifests, indexes) is JSON.

**Tech Stack:** Python 3.12+, Click (CLI framework), xxhash (Python bindings for xxhash3), pytest, pydantic (data validation)

---

## File Structure

```
pyproject.toml                          # Project metadata, dependencies, CLI entry point
games.json                              # Game configuration file
src/downgrade_patcher/__init__.py       # Package init
src/downgrade_patcher/cli.py            # Click CLI commands
src/downgrade_patcher/config.py         # Load and validate games.json
src/downgrade_patcher/hashing.py        # xxhash3 file hashing
src/downgrade_patcher/store.py          # Version store filesystem operations
src/downgrade_patcher/manifest.py       # Manifest and index generation
src/downgrade_patcher/depot.py          # DepotDownloader subprocess wrapper
tests/conftest.py                       # Shared fixtures (tmp stores, sample files)
tests/test_config.py                    # Config loading tests
tests/test_hashing.py                   # Hashing tests
tests/test_store.py                     # Version store tests
tests/test_manifest.py                  # Manifest generation tests
tests/test_depot.py                     # Depot download tests
tests/test_cli.py                       # CLI integration tests
```

---

### Task 1: Project Scaffolding

**Files:**
- Create: `pyproject.toml`
- Create: `src/downgrade_patcher/__init__.py`
- Create: `games.json`

- [ ] **Step 1: Create pyproject.toml**

```toml
[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"

[project]
name = "downgrade-patcher"
version = "0.1.0"
requires-python = ">=3.12"
dependencies = [
    "click>=8.1",
    "pydantic>=2.0",
    "xxhash>=3.4",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.0",
    "pytest-tmp-files>=0.1",
]

[project.scripts]
downgrade-tool = "downgrade_patcher.cli:main"

[tool.hatch.build.targets.wheel]
packages = ["src/downgrade_patcher"]
```

- [ ] **Step 2: Create package init**

```python
# src/downgrade_patcher/__init__.py
```

Empty file — just marks the package.

- [ ] **Step 3: Create initial games.json**

```json
{
  "games": [
    {
      "slug": "skyrim-se",
      "name": "Skyrim Special Edition",
      "steam_app_id": 489830,
      "depot_ids": [489833, 489834],
      "exe_path": "SkyrimSE.exe",
      "best_of_both_worlds": true,
      "nexus_domain": "skyrimspecialedition",
      "nexus_mod_id": 12345
    }
  ]
}
```

- [ ] **Step 4: Install in dev mode and verify**

Run: `cd /home/tbaldrid/oss/downgrade-patcher && pip install -e ".[dev]"`
Expected: Successful install with all dependencies.

- [ ] **Step 5: Commit**

```bash
git add pyproject.toml src/downgrade_patcher/__init__.py games.json
git commit -m "scaffold: project structure with dependencies"
```

---

### Task 2: Game Configuration Loading

**Files:**
- Create: `src/downgrade_patcher/config.py`
- Create: `tests/test_config.py`
- Create: `tests/conftest.py`

- [ ] **Step 1: Write failing test for loading game config**

```python
# tests/conftest.py
import json
import pytest
from pathlib import Path


@pytest.fixture
def sample_games_json(tmp_path: Path) -> Path:
    data = {
        "games": [
            {
                "slug": "skyrim-se",
                "name": "Skyrim Special Edition",
                "steam_app_id": 489830,
                "depot_ids": [489833, 489834],
                "exe_path": "SkyrimSE.exe",
                "best_of_both_worlds": True,
                "nexus_domain": "skyrimspecialedition",
                "nexus_mod_id": 12345,
            },
            {
                "slug": "fallout4",
                "name": "Fallout 4",
                "steam_app_id": 377160,
                "depot_ids": [377162],
                "exe_path": "Fallout4.exe",
                "best_of_both_worlds": False,
                "nexus_domain": "fallout4",
                "nexus_mod_id": 67890,
            },
        ]
    }
    path = tmp_path / "games.json"
    path.write_text(json.dumps(data))
    return path
```

```python
# tests/test_config.py
from downgrade_patcher.config import GameConfig, load_games


def test_load_games_returns_all_games(sample_games_json):
    games = load_games(sample_games_json)
    assert len(games) == 2
    assert games[0].slug == "skyrim-se"
    assert games[1].slug == "fallout4"


def test_game_config_fields(sample_games_json):
    games = load_games(sample_games_json)
    skyrim = games[0]
    assert skyrim.name == "Skyrim Special Edition"
    assert skyrim.steam_app_id == 489830
    assert skyrim.depot_ids == [489833, 489834]
    assert skyrim.exe_path == "SkyrimSE.exe"
    assert skyrim.best_of_both_worlds is True
    assert skyrim.nexus_domain == "skyrimspecialedition"
    assert skyrim.nexus_mod_id == 12345


def test_find_game_by_slug(sample_games_json):
    games = load_games(sample_games_json)
    game = find_game(games, "skyrim-se")
    assert game.slug == "skyrim-se"


def test_find_game_unknown_slug(sample_games_json):
    games = load_games(sample_games_json)
    game = find_game(games, "nonexistent")
    assert game is None
```

Add the missing import at the top of `test_config.py`:

```python
from downgrade_patcher.config import GameConfig, load_games, find_game
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_config.py -v`
Expected: ImportError — `downgrade_patcher.config` does not exist.

- [ ] **Step 3: Implement config module**

```python
# src/downgrade_patcher/config.py
import json
from pathlib import Path

from pydantic import BaseModel


class GameConfig(BaseModel):
    slug: str
    name: str
    steam_app_id: int
    depot_ids: list[int]
    exe_path: str
    best_of_both_worlds: bool
    nexus_domain: str
    nexus_mod_id: int


class GamesFile(BaseModel):
    games: list[GameConfig]


def load_games(path: Path) -> list[GameConfig]:
    data = json.loads(path.read_text())
    return GamesFile.model_validate(data).games


def find_game(games: list[GameConfig], slug: str) -> GameConfig | None:
    for game in games:
        if game.slug == slug:
            return game
    return None
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_config.py -v`
Expected: All 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/config.py tests/conftest.py tests/test_config.py
git commit -m "feat: game configuration loading and validation"
```

---

### Task 3: File Hashing with xxhash3

**Files:**
- Create: `src/downgrade_patcher/hashing.py`
- Create: `tests/test_hashing.py`

- [ ] **Step 1: Write failing test for hashing a file**

```python
# tests/test_hashing.py
from pathlib import Path
from downgrade_patcher.hashing import hash_file


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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_hashing.py -v`
Expected: ImportError — `downgrade_patcher.hashing` does not exist.

- [ ] **Step 3: Implement hashing module**

```python
# src/downgrade_patcher/hashing.py
from pathlib import Path

import xxhash

CHUNK_SIZE = 1024 * 1024  # 1MB read chunks


def hash_file(path: Path) -> str:
    h = xxhash.xxh3_64()
    with open(path, "rb") as f:
        while chunk := f.read(CHUNK_SIZE):
            h.update(chunk)
    return h.hexdigest()
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_hashing.py -v`
Expected: All 3 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/hashing.py tests/test_hashing.py
git commit -m "feat: xxhash3 file hashing"
```

---

### Task 4: Version Store — Path Management and Ingest

**Files:**
- Create: `src/downgrade_patcher/store.py`
- Create: `tests/test_store.py`

- [ ] **Step 1: Write failing tests for store path helpers**

```python
# tests/test_store.py
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_store.py -v`
Expected: ImportError — `downgrade_patcher.store` does not exist.

- [ ] **Step 3: Implement store path helpers**

```python
# src/downgrade_patcher/store.py
import json
import shutil
from pathlib import Path


class VersionStore:
    def __init__(self, root: Path):
        self.root = root

    def version_dir(self, game_slug: str, version: str) -> Path:
        return self.root / game_slug / version

    def manifest_path(self, game_slug: str, version: str) -> Path:
        return self.root / game_slug / "manifests" / f"{version}.json"

    def manifest_index_path(self, game_slug: str) -> Path:
        return self.root / game_slug / "manifest-index.json"

    def hash_index_path(self, game_slug: str) -> Path:
        return self.root / game_slug / "hash-index.json"

    def list_versions(self, game_slug: str) -> list[str]:
        game_dir = self.root / game_slug
        if not game_dir.exists():
            return []
        skip = {"manifests"}
        return sorted(
            d.name
            for d in game_dir.iterdir()
            if d.is_dir() and d.name not in skip
        )
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_store.py -v`
Expected: All 5 tests PASS.

- [ ] **Step 5: Write failing test for ingest**

Add to `tests/test_store.py`:

```python
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
```

- [ ] **Step 6: Run tests to verify new tests fail**

Run: `pytest tests/test_store.py::test_ingest_copies_files tests/test_store.py::test_ingest_appears_in_list_versions tests/test_store.py::test_ingest_rejects_duplicate_version -v`
Expected: AttributeError — `VersionStore` has no `ingest` method.

- [ ] **Step 7: Implement ingest**

Add to `src/downgrade_patcher/store.py` inside the `VersionStore` class:

```python
    def ingest(self, game_slug: str, version: str, depot_path: Path) -> Path:
        dest = self.version_dir(game_slug, version)
        if dest.exists():
            raise FileExistsError(f"Version {version} already exists for {game_slug}")
        shutil.copytree(depot_path, dest)
        return dest
```

- [ ] **Step 8: Run all store tests**

Run: `pytest tests/test_store.py -v`
Expected: All 8 tests PASS.

- [ ] **Step 9: Commit**

```bash
git add src/downgrade_patcher/store.py tests/test_store.py
git commit -m "feat: version store with path management and ingest"
```

---

### Task 5: Manifest Generation

**Files:**
- Create: `src/downgrade_patcher/manifest.py`
- Create: `tests/test_manifest.py`

- [ ] **Step 1: Write failing test for manifest generation**

```python
# tests/test_manifest.py
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_manifest.py -v`
Expected: ImportError — `downgrade_patcher.manifest` does not exist.

- [ ] **Step 3: Implement manifest generation**

```python
# src/downgrade_patcher/manifest.py
from pathlib import Path

from downgrade_patcher.hashing import hash_file


def generate_manifest(game_slug: str, version: str, version_dir: Path) -> dict:
    files = []
    for file_path in sorted(version_dir.rglob("*")):
        if not file_path.is_file():
            continue
        relative = file_path.relative_to(version_dir)
        files.append({
            "path": relative.as_posix(),
            "size": file_path.stat().st_size,
            "xxhash3": hash_file(file_path),
        })
    return {
        "game": game_slug,
        "version": version,
        "files": files,
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_manifest.py -v`
Expected: All 3 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/manifest.py tests/test_manifest.py
git commit -m "feat: manifest generation from version directory"
```

---

### Task 6: Manifest Index Generation

**Files:**
- Modify: `src/downgrade_patcher/manifest.py`
- Modify: `tests/test_manifest.py`

- [ ] **Step 1: Write failing test for manifest index**

Add to `tests/test_manifest.py`:

```python
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


def test_build_manifest_index(tmp_path: Path):
    # Create two "versions" with different exe content
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
    # Each exe hash should map to its version
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

    # Should only have one entry (the exe), not the dll
    assert len(index["versions"]) == 1
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_manifest.py::test_build_manifest_index tests/test_manifest.py::test_manifest_index_uses_exe_path_from_config -v`
Expected: ImportError — `build_manifest_index` not found.

- [ ] **Step 3: Implement manifest index builder**

Add to `src/downgrade_patcher/manifest.py`:

```python
from downgrade_patcher.config import GameConfig


def build_manifest_index(
    game_slug: str,
    game_config: GameConfig,
    manifests: dict[str, dict],
) -> dict:
    versions = {}
    for version, manifest in manifests.items():
        for file_entry in manifest["files"]:
            if file_entry["path"] == game_config.exe_path:
                versions[file_entry["xxhash3"]] = version
                break
    return {
        "game": game_slug,
        "versions": versions,
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_manifest.py -v`
Expected: All 5 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/manifest.py tests/test_manifest.py
git commit -m "feat: manifest index mapping exe hashes to versions"
```

---

### Task 7: Hash-to-File Index

**Files:**
- Modify: `src/downgrade_patcher/manifest.py`
- Modify: `tests/test_manifest.py`

- [ ] **Step 1: Write failing test for hash index**

Add to `tests/test_manifest.py`:

```python
from downgrade_patcher.manifest import (
    generate_manifest,
    build_manifest_index,
    build_hash_index,
)


def test_build_hash_index(tmp_path: Path):
    v1_dir = tmp_path / "skyrim-se" / "1.5.97"
    v1_dir.mkdir(parents=True)
    (v1_dir / "game.exe").write_bytes(b"exe-v1")
    (v1_dir / "Data").mkdir()
    (v1_dir / "Data" / "main.bsa").write_bytes(b"bsa-v1")

    manifest = generate_manifest("skyrim-se", "1.5.97", v1_dir)
    manifests = {"1.5.97": manifest}

    store_root = tmp_path
    index = build_hash_index("skyrim-se", manifests, store_root)

    # Every file hash should map to at least one store path
    for file_entry in manifest["files"]:
        assert file_entry["xxhash3"] in index
        paths = index[file_entry["xxhash3"]]
        assert len(paths) >= 1
        # Each path should be a real file
        for p in paths:
            assert Path(p).exists()


def test_hash_index_deduplicates_across_versions(tmp_path: Path):
    # Same file content in two versions -> hash maps to both paths
    v1_dir = tmp_path / "skyrim-se" / "1.5.97"
    v1_dir.mkdir(parents=True)
    (v1_dir / "shared.dll").write_bytes(b"same-content")

    v2_dir = tmp_path / "skyrim-se" / "1.6.1170"
    v2_dir.mkdir(parents=True)
    (v2_dir / "shared.dll").write_bytes(b"same-content")

    m1 = generate_manifest("skyrim-se", "1.5.97", v1_dir)
    m2 = generate_manifest("skyrim-se", "1.6.1170", v2_dir)
    manifests = {"1.5.97": m1, "1.6.1170": m2}

    store_root = tmp_path
    index = build_hash_index("skyrim-se", manifests, store_root)

    dll_hash = m1["files"][0]["xxhash3"]
    assert len(index[dll_hash]) == 2
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_manifest.py::test_build_hash_index tests/test_manifest.py::test_hash_index_deduplicates_across_versions -v`
Expected: ImportError — `build_hash_index` not found.

- [ ] **Step 3: Implement hash index builder**

Add to `src/downgrade_patcher/manifest.py`:

```python
def build_hash_index(
    game_slug: str,
    manifests: dict[str, dict],
    store_root: Path,
) -> dict[str, list[str]]:
    index: dict[str, list[str]] = {}
    for version, manifest in manifests.items():
        version_dir = store_root / game_slug / version
        for file_entry in manifest["files"]:
            file_hash = file_entry["xxhash3"]
            abs_path = str(version_dir / file_entry["path"])
            if file_hash not in index:
                index[file_hash] = []
            if abs_path not in index[file_hash]:
                index[file_hash].append(abs_path)
    return index
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_manifest.py -v`
Expected: All 7 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/manifest.py tests/test_manifest.py
git commit -m "feat: hash-to-file index for patch service lookups"
```

---

### Task 8: Depot Download Wrapper

**Files:**
- Create: `src/downgrade_patcher/depot.py`
- Create: `tests/test_depot.py`

- [ ] **Step 1: Write failing test for depot download**

```python
# tests/test_depot.py
from pathlib import Path
from unittest.mock import patch, call
from downgrade_patcher.depot import download_depot


def test_download_depot_calls_depotdownloader(tmp_path: Path):
    output_dir = tmp_path / "output"

    with patch("downgrade_patcher.depot.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 0
        download_depot(
            app_id=489830,
            depot_id=489833,
            manifest_id="abc123",
            output_dir=output_dir,
        )

    mock_run.assert_called_once()
    args = mock_run.call_args
    cmd = args[0][0]
    assert "DepotDownloader" in cmd[0] or "depotdownloader" in cmd[0].lower()
    cmd_str = " ".join(cmd)
    assert "489830" in cmd_str
    assert "489833" in cmd_str
    assert "abc123" in cmd_str


def test_download_depot_raises_on_failure(tmp_path: Path):
    import pytest

    with patch("downgrade_patcher.depot.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 1
        mock_run.return_value.stderr = "auth failed"

        with pytest.raises(RuntimeError, match="DepotDownloader failed"):
            download_depot(
                app_id=489830,
                depot_id=489833,
                manifest_id="abc123",
                output_dir=tmp_path / "output",
            )
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_depot.py -v`
Expected: ImportError — `downgrade_patcher.depot` does not exist.

- [ ] **Step 3: Implement depot wrapper**

```python
# src/downgrade_patcher/depot.py
import subprocess
from pathlib import Path


def download_depot(
    app_id: int,
    depot_id: int,
    manifest_id: str,
    output_dir: Path,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [
            "DepotDownloader",
            "-app", str(app_id),
            "-depot", str(depot_id),
            "-manifest", manifest_id,
            "-dir", str(output_dir),
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"DepotDownloader failed (exit {result.returncode}): {result.stderr}"
        )
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_depot.py -v`
Expected: All 2 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/depot.py tests/test_depot.py
git commit -m "feat: DepotDownloader subprocess wrapper"
```

---

### Task 9: CLI — Ingest Command (Full Pipeline)

**Files:**
- Create: `src/downgrade_patcher/cli.py`
- Create: `tests/test_cli.py`

- [ ] **Step 1: Write failing test for ingest command**

```python
# tests/test_cli.py
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_cli.py -v`
Expected: ImportError — `downgrade_patcher.cli` does not exist.

- [ ] **Step 3: Implement CLI with ingest command**

```python
# src/downgrade_patcher/cli.py
import json
from pathlib import Path

import click

from downgrade_patcher.config import load_games, find_game
from downgrade_patcher.manifest import (
    generate_manifest,
    build_manifest_index,
    build_hash_index,
)
from downgrade_patcher.store import VersionStore


@click.group()
@click.option(
    "--store-root",
    type=click.Path(path_type=Path),
    default=Path("store"),
    help="Root directory of the version store",
)
@click.option(
    "--games-config",
    type=click.Path(exists=True, path_type=Path),
    default=Path("games.json"),
    help="Path to games.json",
)
@click.pass_context
def main(ctx: click.Context, store_root: Path, games_config: Path):
    ctx.ensure_object(dict)
    ctx.obj["store"] = VersionStore(store_root)
    ctx.obj["games"] = load_games(games_config)


@main.command()
@click.option("--game", required=True, help="Game slug")
@click.option("--version", "game_version", required=True, help="Version label")
@click.option("--depot-path", required=True, type=click.Path(exists=True, path_type=Path))
@click.pass_context
def ingest(ctx: click.Context, game: str, game_version: str, depot_path: Path):
    store: VersionStore = ctx.obj["store"]
    games = ctx.obj["games"]

    game_config = find_game(games, game)
    if game_config is None:
        raise click.ClickException(f"Unknown game: {game}")

    # Copy files into version store
    click.echo(f"Ingesting {game} {game_version} from {depot_path}")
    store.ingest(game, game_version, depot_path)

    # Generate manifest for this version
    version_dir = store.version_dir(game, game_version)
    manifest = generate_manifest(game, game_version, version_dir)
    manifest_path = store.manifest_path(game, game_version)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2))
    click.echo(f"Manifest written: {manifest_path}")

    # Rebuild manifest index (all versions)
    all_manifests = _load_all_manifests(store, game)
    index = build_manifest_index(game, game_config, all_manifests)
    index_path = store.manifest_index_path(game)
    index_path.write_text(json.dumps(index, indent=2))
    click.echo(f"Manifest index updated: {index_path}")

    # Rebuild hash index (all versions)
    hash_index = build_hash_index(game, all_manifests, store.root)
    hash_index_path = store.hash_index_path(game)
    hash_index_path.write_text(json.dumps(hash_index, indent=2))
    click.echo(f"Hash index updated: {hash_index_path}")


def _load_all_manifests(store: VersionStore, game_slug: str) -> dict[str, dict]:
    manifests = {}
    manifests_dir = store.manifest_path(game_slug, "dummy").parent
    if not manifests_dir.exists():
        return manifests
    for path in manifests_dir.glob("*.json"):
        data = json.loads(path.read_text())
        manifests[data["version"]] = data
    return manifests
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_cli.py -v`
Expected: All 1 test PASS.

- [ ] **Step 5: Write test for ingesting a second version updates indexes**

Add to `tests/test_cli.py`:

```python
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
```

- [ ] **Step 6: Run new test to verify it passes (implementation already handles this)**

Run: `pytest tests/test_cli.py -v`
Expected: All 2 tests PASS.

- [ ] **Step 7: Commit**

```bash
git add src/downgrade_patcher/cli.py tests/test_cli.py
git commit -m "feat: CLI ingest command with manifest and index generation"
```

---

### Task 10: CLI — List Versions Command

**Files:**
- Modify: `src/downgrade_patcher/cli.py`
- Modify: `tests/test_cli.py`

- [ ] **Step 1: Write failing test for list-versions**

Add to `tests/test_cli.py`:

```python
def test_list_versions(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

    # Ingest two versions
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pytest tests/test_cli.py::test_list_versions tests/test_cli.py::test_list_versions_empty -v`
Expected: Error — `list-versions` command not found.

- [ ] **Step 3: Implement list-versions command**

Add to `src/downgrade_patcher/cli.py`:

```python
@main.command("list-versions")
@click.option("--game", required=True, help="Game slug")
@click.pass_context
def list_versions(ctx: click.Context, game: str):
    store: VersionStore = ctx.obj["store"]
    versions = store.list_versions(game)
    if not versions:
        click.echo(f"No versions found for {game}")
        return
    click.echo(f"Versions for {game}:")
    for v in versions:
        click.echo(f"  {v}")
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_cli.py -v`
Expected: All 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/cli.py tests/test_cli.py
git commit -m "feat: CLI list-versions command"
```

---

### Task 11: CLI — Download Command

**Files:**
- Modify: `src/downgrade_patcher/cli.py`
- Modify: `tests/test_cli.py`

- [ ] **Step 1: Write failing test for download command**

Add to `tests/test_cli.py`:

```python
from unittest.mock import patch as mock_patch


def test_download_calls_depot_downloader(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

    with mock_patch("downgrade_patcher.cli.download_depot") as mock_dl:
        result = runner.invoke(main, [
            "--store-root", str(store_root),
            "--games-config", str(sample_games_json),
            "download",
            "--game", "skyrim-se",
            "--depot", "489833",
            "--manifest", "abc123",
            "--output-dir", str(tmp_path / "staging"),
        ])

    assert result.exit_code == 0, result.output
    mock_dl.assert_called_once_with(
        app_id=489830,
        depot_id=489833,
        manifest_id="abc123",
        output_dir=tmp_path / "staging",
    )
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_cli.py::test_download_calls_depot_downloader -v`
Expected: Error — `download` command not found.

- [ ] **Step 3: Implement download command**

Add to `src/downgrade_patcher/cli.py`:

```python
from downgrade_patcher.depot import download_depot


@main.command()
@click.option("--game", required=True, help="Game slug")
@click.option("--depot", "depot_id", required=True, type=int, help="Steam depot ID")
@click.option("--manifest", "manifest_id", required=True, help="Steam manifest ID")
@click.option(
    "--output-dir",
    required=True,
    type=click.Path(path_type=Path),
    help="Download destination",
)
@click.pass_context
def download(
    ctx: click.Context,
    game: str,
    depot_id: int,
    manifest_id: str,
    output_dir: Path,
):
    games = ctx.obj["games"]
    game_config = find_game(games, game)
    if game_config is None:
        raise click.ClickException(f"Unknown game: {game}")

    click.echo(f"Downloading depot {depot_id} (manifest {manifest_id}) for {game}")
    download_depot(
        app_id=game_config.steam_app_id,
        depot_id=depot_id,
        manifest_id=manifest_id,
        output_dir=output_dir,
    )
    click.echo(f"Download complete: {output_dir}")
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_cli.py -v`
Expected: All 5 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/cli.py tests/test_cli.py
git commit -m "feat: CLI download command wrapping DepotDownloader"
```

---

### Task 12: CLI — Warm Cache Command

**Files:**
- Modify: `src/downgrade_patcher/cli.py`
- Modify: `tests/test_cli.py`

- [ ] **Step 1: Write failing test for warm-cache**

Add to `tests/test_cli.py`:

```python
import subprocess


def test_warm_cache_generates_patches(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

    # Ingest two versions with different exe content
    for ver, content in [("1.5.97", b"exe-old-content"), ("1.6.1170", b"exe-new-content")]:
        depot = tmp_path / f"depot_{ver}"
        depot.mkdir()
        (depot / "SkyrimSE.exe").write_bytes(content)
        runner.invoke(main, [
            "--store-root", str(store_root),
            "--games-config", str(sample_games_json),
            "ingest", "--game", "skyrim-se", "--version", ver,
            "--depot-path", str(depot),
        ])

    cache_root = tmp_path / "cache"

    with mock_patch("downgrade_patcher.cli.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 0
        result = runner.invoke(main, [
            "--store-root", str(store_root),
            "--games-config", str(sample_games_json),
            "warm-cache",
            "--game", "skyrim-se",
            "--from-version", "1.6.1170",
            "--to-version", "1.5.97",
            "--cache-root", str(cache_root),
        ])

    assert result.exit_code == 0, result.output
    # Should have called zstd for the exe (the only file that differs)
    assert mock_run.call_count >= 1
    cmd_str = " ".join(mock_run.call_args_list[0][0][0])
    assert "--patch-from" in cmd_str
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/test_cli.py::test_warm_cache_generates_patches -v`
Expected: Error — `warm-cache` command not found.

- [ ] **Step 3: Implement warm-cache command**

Add to `src/downgrade_patcher/cli.py`:

```python
import subprocess


@main.command("warm-cache")
@click.option("--game", required=True, help="Game slug")
@click.option("--from-version", required=True, help="Source version")
@click.option("--to-version", required=True, help="Target version")
@click.option(
    "--cache-root",
    type=click.Path(path_type=Path),
    default=Path("cache"),
    help="Root directory of the patch cache",
)
@click.option("--chunk-size", type=int, default=8 * 1024 * 1024, help="Chunk size in bytes")
@click.pass_context
def warm_cache(
    ctx: click.Context,
    game: str,
    from_version: str,
    to_version: str,
    cache_root: Path,
    chunk_size: int,
):
    store: VersionStore = ctx.obj["store"]

    from_manifest_path = store.manifest_path(game, from_version)
    to_manifest_path = store.manifest_path(game, to_version)
    if not from_manifest_path.exists() or not to_manifest_path.exists():
        raise click.ClickException("Both versions must be ingested first")

    from_manifest = json.loads(from_manifest_path.read_text())
    to_manifest = json.loads(to_manifest_path.read_text())

    # Build lookup: hash -> file entry for source
    from_by_hash = {f["xxhash3"]: f for f in from_manifest["files"]}
    to_by_hash = {f["xxhash3"]: f for f in to_manifest["files"]}

    from_dir = store.version_dir(game, from_version)
    to_dir = store.version_dir(game, to_version)

    generated = 0
    for to_entry in to_manifest["files"]:
        to_hash = to_entry["xxhash3"]
        if to_hash in from_by_hash:
            continue  # File unchanged, no patch needed

        # Find the best source file to patch from: prefer exact path match
        source_entry = next(
            (f for f in from_manifest["files"] if f["path"] == to_entry["path"]),
            None,
        )

        if source_entry is None:
            click.echo(f"  Skipping {to_entry['path']}: no source file to patch from")
            continue

        source_file = from_dir / source_entry["path"]
        target_file = to_dir / to_entry["path"]
        source_hash = source_entry["xxhash3"]

        patch_dir = cache_root / game / source_hash / to_hash
        if patch_dir.exists():
            click.echo(f"  Already cached: {to_entry['path']}")
            continue

        patch_dir.mkdir(parents=True, exist_ok=True)
        patch_file = patch_dir / "patch.zst"

        click.echo(f"  Generating patch: {to_entry['path']}")
        result = subprocess.run(
            [
                "zstd",
                "--patch-from", str(source_file),
                str(target_file),
                "-o", str(patch_file),
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            click.echo(f"    zstd failed: {result.stderr}")
            continue

        # Split into chunks
        _split_into_chunks(patch_file, patch_dir, chunk_size)
        patch_file.unlink()  # Remove the unsplit patch
        generated += 1

    click.echo(f"Generated {generated} patches")


def _split_into_chunks(patch_file: Path, patch_dir: Path, chunk_size: int) -> None:
    chunks = []
    chunk_index = 0
    with open(patch_file, "rb") as f:
        while True:
            data = f.read(chunk_size)
            if not data:
                break
            chunk_path = patch_dir / f"chunk-{chunk_index}"
            chunk_path.write_bytes(data)
            chunks.append({"index": chunk_index, "size": len(data)})
            chunk_index += 1

    meta = {"total_chunks": len(chunks), "chunks": chunks}
    (patch_dir / "meta.json").write_text(json.dumps(meta, indent=2))
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pytest tests/test_cli.py -v`
Expected: All 6 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/downgrade_patcher/cli.py tests/test_cli.py
git commit -m "feat: CLI warm-cache command for pre-generating patches"
```

---

### Task 13: Run Full Test Suite and Verify

**Files:** None (verification only)

- [ ] **Step 1: Run the complete test suite**

Run: `pytest tests/ -v`
Expected: All tests PASS (should be ~20 tests total).

- [ ] **Step 2: Verify CLI entry point works**

Run: `downgrade-tool --help`
Expected: Shows help with commands: `download`, `ingest`, `list-versions`, `warm-cache`.

- [ ] **Step 3: Commit any final fixes if needed**

If any tests fail, fix and commit.
