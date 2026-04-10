# Patch Service Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the FastAPI patch service that lazily generates and serves chunked zstd patches, plus extract the shared `downgrade_common` library from the existing CLI code.

**Architecture:** Monorepo with three subprojects: `common/` (shared library), `cli/` (existing CLI refactored), `server/` (new FastAPI service). The server loads all metadata into memory at startup, serves manifests and patch chunks, and generates patches lazily on first request using per-chunk zstd with the whole source file as dictionary.

**Tech Stack:** Python 3.12+, FastAPI, uvicorn, pydantic, xxhash, zstd CLI, pytest, httpx (test client)

---

## File Structure

```
common/
  pyproject.toml
  src/downgrade_common/__init__.py
  src/downgrade_common/config.py         # GameConfig, load_games, find_game (from CLI)
  src/downgrade_common/hashing.py        # hash_file (from CLI)
  src/downgrade_common/chunking.py       # chunk target files, per-chunk zstd patch gen
  src/downgrade_common/cache.py          # cache path layout, meta.json read/write
  tests/test_config.py                   # (moved from root tests/)
  tests/test_hashing.py                  # (moved from root tests/)
  tests/test_chunking.py
  tests/test_cache.py

cli/
  pyproject.toml
  src/downgrade_patcher/__init__.py
  src/downgrade_patcher/cli.py           # refactored to use downgrade_common
  src/downgrade_patcher/store.py         # unchanged
  src/downgrade_patcher/manifest.py      # refactored imports
  src/downgrade_patcher/depot.py         # unchanged
  tests/conftest.py
  tests/test_cli.py                      # (moved, adjusted imports)
  tests/test_store.py                    # (moved)
  tests/test_manifest.py                 # (moved, adjusted imports)
  tests/test_depot.py                    # (moved)

server/
  pyproject.toml
  src/patch_server/__init__.py
  src/patch_server/app.py                # FastAPI app, lifespan
  src/patch_server/settings.py           # pydantic Settings for env vars
  src/patch_server/store_reader.py       # in-memory store reader
  src/patch_server/patch_gen.py          # lazy patch generation with locking
  src/patch_server/routes/__init__.py
  src/patch_server/routes/games.py       # GET /api/games
  src/patch_server/routes/manifests.py   # manifest endpoints
  src/patch_server/routes/patches.py     # patch chunk endpoints
  tests/conftest.py                      # shared fixtures, test client
  tests/test_store_reader.py
  tests/test_patch_gen.py
  tests/test_routes_games.py
  tests/test_routes_manifests.py
  tests/test_routes_patches.py

config/
  games.json                             # (moved from root)
```

---

### Task 1: Create `downgrade_common` Package with Config and Hashing

**Files:**
- Create: `common/pyproject.toml`
- Create: `common/src/downgrade_common/__init__.py`
- Create: `common/src/downgrade_common/config.py`
- Create: `common/src/downgrade_common/hashing.py`
- Create: `common/tests/test_config.py`
- Create: `common/tests/test_hashing.py`

- [ ] **Step 1: Create common/pyproject.toml**

```toml
[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"

[project]
name = "downgrade-common"
version = "0.1.0"
requires-python = ">=3.12"
dependencies = [
    "pydantic>=2.0",
    "xxhash>=3.4",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.0",
]

[tool.hatch.build.targets.wheel]
packages = ["src/downgrade_common"]
```

- [ ] **Step 2: Create package init**

```python
# common/src/downgrade_common/__init__.py
```

- [ ] **Step 3: Copy config.py from CLI to common**

Copy `src/downgrade_patcher/config.py` to `common/src/downgrade_common/config.py` — the content is identical:

```python
# common/src/downgrade_common/config.py
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

- [ ] **Step 4: Copy hashing.py from CLI to common**

Copy `src/downgrade_patcher/hashing.py` to `common/src/downgrade_common/hashing.py` — the content is identical:

```python
# common/src/downgrade_common/hashing.py
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

- [ ] **Step 5: Create tests for config (adapted from existing)**

```python
# common/tests/test_config.py
import json
import pytest
from pathlib import Path
from downgrade_common.config import GameConfig, load_games, find_game


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

- [ ] **Step 6: Create tests for hashing (adapted from existing)**

```python
# common/tests/test_hashing.py
from pathlib import Path
from downgrade_common.hashing import hash_file


def test_hash_file_returns_hex_string(tmp_path: Path):
    f = tmp_path / "test.bin"
    f.write_bytes(b"hello world")
    result = hash_file(f)
    assert isinstance(result, str)
    assert len(result) == 16


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

- [ ] **Step 7: Install common in dev mode and run tests**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pip install -e ".[dev]"`
Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/ -v`
Expected: All 7 tests PASS.

- [ ] **Step 8: Commit**

```bash
git add common/
git commit -m "feat: extract downgrade_common package with config and hashing"
```

---

### Task 2: Add Cache Module to `downgrade_common`

**Files:**
- Create: `common/src/downgrade_common/cache.py`
- Create: `common/tests/test_cache.py`

- [ ] **Step 1: Write failing tests for cache path helpers and meta read/write**

```python
# common/tests/test_cache.py
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/test_cache.py -v`
Expected: ImportError.

- [ ] **Step 3: Implement cache module**

```python
# common/src/downgrade_common/cache.py
import json
from pathlib import Path

from pydantic import BaseModel


class ChunkMeta(BaseModel):
    index: int
    size: int


class PatchMeta(BaseModel):
    total_chunks: int
    chunks: list[ChunkMeta]


class CacheLayout:
    def __init__(self, root: Path):
        self.root = root

    def patch_dir(self, game_slug: str, source_hash: str, target_hash: str) -> Path:
        return self.root / game_slug / source_hash / target_hash

    def meta_path(self, game_slug: str, source_hash: str, target_hash: str) -> Path:
        return self.patch_dir(game_slug, source_hash, target_hash) / "meta.json"

    def chunk_path(
        self, game_slug: str, source_hash: str, target_hash: str, index: int
    ) -> Path:
        return self.patch_dir(game_slug, source_hash, target_hash) / str(index)

    def is_cached(self, game_slug: str, source_hash: str, target_hash: str) -> bool:
        return self.meta_path(game_slug, source_hash, target_hash).exists()

    def write_meta(
        self, game_slug: str, source_hash: str, target_hash: str, meta: PatchMeta
    ) -> None:
        path = self.meta_path(game_slug, source_hash, target_hash)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(meta.model_dump_json(indent=2))

    def read_meta(
        self, game_slug: str, source_hash: str, target_hash: str
    ) -> PatchMeta | None:
        path = self.meta_path(game_slug, source_hash, target_hash)
        if not path.exists():
            return None
        return PatchMeta.model_validate_json(path.read_text())
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/test_cache.py -v`
Expected: All 7 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add common/src/downgrade_common/cache.py common/tests/test_cache.py
git commit -m "feat: cache layout and meta read/write in downgrade_common"
```

---

### Task 3: Add Chunking Module to `downgrade_common`

**Files:**
- Create: `common/src/downgrade_common/chunking.py`
- Create: `common/tests/test_chunking.py`

- [ ] **Step 1: Write failing test for splitting a target file into chunks**

```python
# common/tests/test_chunking.py
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
    # Write 2.5 chunks worth of data
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
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/test_chunking.py -v`
Expected: ImportError.

- [ ] **Step 3: Implement split_target_into_chunks**

```python
# common/src/downgrade_common/chunking.py
import subprocess
import tempfile
from pathlib import Path

CHUNK_SIZE = 8 * 1024 * 1024  # 8MB


def split_target_into_chunks(target_path: Path, output_dir: Path) -> list[Path]:
    output_dir.mkdir(parents=True, exist_ok=True)
    chunk_paths = []
    chunk_index = 0
    with open(target_path, "rb") as f:
        while True:
            data = f.read(CHUNK_SIZE)
            if not data:
                break
            chunk_path = output_dir / str(chunk_index)
            chunk_path.write_bytes(data)
            chunk_paths.append(chunk_path)
            chunk_index += 1
    return chunk_paths
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/test_chunking.py -v`
Expected: All 3 tests PASS.

- [ ] **Step 5: Write failing test for generate_chunked_patches**

Add to `common/tests/test_chunking.py`:

```python
import json
from unittest.mock import patch as mock_patch, call
from downgrade_common.chunking import generate_chunked_patches
from downgrade_common.cache import CacheLayout


def test_generate_chunked_patches_calls_zstd_per_chunk(tmp_path: Path):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source-dictionary-content")
    target = tmp_path / "target.bin"
    target.write_bytes(b"A" * CHUNK_SIZE + b"B" * 100)  # 2 chunks

    cache = CacheLayout(tmp_path / "cache")

    with mock_patch("downgrade_common.chunking.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 0
        # Create fake patch files so meta gets written
        def side_effect(cmd, **kwargs):
            # The -o flag value is the output path
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
    # Each call should use --patch-from with the whole source
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

    # Meta should be readable from cache
    loaded = cache.read_meta("skyrim-se", "aaa111", "bbb222")
    assert loaded is not None
    assert loaded.total_chunks == 1

    # Chunk file should exist
    chunk_path = cache.chunk_path("skyrim-se", "aaa111", "bbb222", 0)
    assert chunk_path.exists()
```

- [ ] **Step 6: Run tests to verify new tests fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/test_chunking.py::test_generate_chunked_patches_calls_zstd_per_chunk tests/test_chunking.py::test_generate_chunked_patches_writes_cache -v`
Expected: ImportError — `generate_chunked_patches` not found.

- [ ] **Step 7: Implement generate_chunked_patches**

Add to `common/src/downgrade_common/chunking.py`:

```python
from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta


def generate_chunked_patches(
    source_path: Path,
    target_path: Path,
    game_slug: str,
    source_hash: str,
    target_hash: str,
    cache: CacheLayout,
) -> PatchMeta:
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp = Path(tmp_dir)

        # Split target into chunks
        chunks_dir = tmp / "target_chunks"
        chunk_paths = split_target_into_chunks(target_path, chunks_dir)

        # Generate a zstd patch for each chunk using whole source as dictionary
        patch_dir = cache.patch_dir(game_slug, source_hash, target_hash)
        patch_dir.mkdir(parents=True, exist_ok=True)

        chunk_metas = []
        for i, target_chunk in enumerate(chunk_paths):
            patch_output = patch_dir / str(i)
            result = subprocess.run(
                [
                    "zstd",
                    "--patch-from", str(source_path),
                    str(target_chunk),
                    "-o", str(patch_output),
                ],
                capture_output=True,
                text=True,
            )
            if result.returncode != 0:
                # Clean up on failure
                import shutil
                shutil.rmtree(patch_dir, ignore_errors=True)
                raise RuntimeError(
                    f"zstd failed for chunk {i}: {result.stderr}"
                )
            chunk_metas.append(ChunkMeta(index=i, size=patch_output.stat().st_size))

        meta = PatchMeta(total_chunks=len(chunk_metas), chunks=chunk_metas)
        cache.write_meta(game_slug, source_hash, target_hash, meta)
        return meta
```

- [ ] **Step 8: Run all chunking tests**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/test_chunking.py -v`
Expected: All 5 tests PASS.

- [ ] **Step 9: Commit**

```bash
git add common/src/downgrade_common/chunking.py common/tests/test_chunking.py
git commit -m "feat: chunked patch generation in downgrade_common"
```

---

### Task 4: Refactor CLI to Use `downgrade_common`

**Files:**
- Create: `cli/pyproject.toml`
- Move: `src/downgrade_patcher/` → `cli/src/downgrade_patcher/`
- Move: `tests/` → `cli/tests/`
- Move: `games.json` → `config/games.json`
- Modify: `cli/src/downgrade_patcher/cli.py` (update imports, use downgrade_common)
- Modify: `cli/src/downgrade_patcher/manifest.py` (update imports)
- Delete: `cli/src/downgrade_patcher/config.py` (replaced by downgrade_common)
- Delete: `cli/src/downgrade_patcher/hashing.py` (replaced by downgrade_common)
- Delete: root `pyproject.toml`

- [ ] **Step 1: Create config/ directory and move games.json**

```bash
mkdir -p config
mv games.json config/games.json
```

- [ ] **Step 2: Create cli/ directory structure and move files**

```bash
mkdir -p cli/src cli/tests
mv src/downgrade_patcher cli/src/downgrade_patcher
mv tests/*.py cli/tests/
rmdir tests
```

- [ ] **Step 3: Delete cli/src/downgrade_patcher/config.py and hashing.py**

These are now in `downgrade_common`. Delete them:

```bash
rm cli/src/downgrade_patcher/config.py
rm cli/src/downgrade_patcher/hashing.py
```

- [ ] **Step 4: Create cli/pyproject.toml**

```toml
[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"

[project]
name = "downgrade-patcher-cli"
version = "0.1.0"
requires-python = ">=3.12"
dependencies = [
    "click>=8.1",
    "downgrade-common",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.0",
]

[project.scripts]
downgrade-tool = "downgrade_patcher.cli:main"

[tool.hatch.build.targets.wheel]
packages = ["src/downgrade_patcher"]
```

- [ ] **Step 5: Delete old root pyproject.toml**

```bash
rm pyproject.toml
```

- [ ] **Step 6: Update cli/src/downgrade_patcher/manifest.py imports**

Replace:

```python
from downgrade_patcher.hashing import hash_file
from downgrade_patcher.config import GameConfig
```

With:

```python
from downgrade_common.hashing import hash_file
from downgrade_common.config import GameConfig
```

- [ ] **Step 7: Update cli/src/downgrade_patcher/cli.py imports and warm-cache**

Replace the imports at the top:

```python
from downgrade_patcher.config import load_games, find_game
```

With:

```python
from downgrade_common.config import load_games, find_game
from downgrade_common.cache import CacheLayout
from downgrade_common.chunking import generate_chunked_patches
```

Replace the `warm_cache` command's patch generation logic. The old code (lines 159-208) used inline `_split_into_chunks` and a single `zstd --patch-from`. Replace the body of `warm_cache` with:

```python
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
@click.pass_context
def warm_cache(
    ctx: click.Context,
    game: str,
    from_version: str,
    to_version: str,
    cache_root: Path,
):
    store: VersionStore = ctx.obj["store"]
    cache = CacheLayout(cache_root)

    from_manifest_path = store.manifest_path(game, from_version)
    to_manifest_path = store.manifest_path(game, to_version)
    if not from_manifest_path.exists() or not to_manifest_path.exists():
        raise click.ClickException("Both versions must be ingested first")

    from_manifest = json.loads(from_manifest_path.read_text())
    to_manifest = json.loads(to_manifest_path.read_text())

    from_by_hash = {f["xxhash3"]: f for f in from_manifest["files"]}

    from_dir = store.version_dir(game, from_version)
    to_dir = store.version_dir(game, to_version)

    generated = 0
    for to_entry in to_manifest["files"]:
        to_hash = to_entry["xxhash3"]
        if to_hash in from_by_hash:
            continue

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

        if cache.is_cached(game, source_hash, to_hash):
            click.echo(f"  Already cached: {to_entry['path']}")
            continue

        click.echo(f"  Generating patch: {to_entry['path']}")
        generate_chunked_patches(
            source_path=source_file,
            target_path=target_file,
            game_slug=game,
            source_hash=source_hash,
            target_hash=to_hash,
            cache=cache,
        )
        generated += 1

    click.echo(f"Generated {generated} patches")
```

Also remove the old `_split_into_chunks` function and the `import subprocess` (no longer needed in cli.py).

- [ ] **Step 8: Update cli/tests/test_config.py imports**

Replace:

```python
from downgrade_patcher.config import GameConfig, load_games, find_game
```

With:

```python
from downgrade_common.config import GameConfig, load_games, find_game
```

- [ ] **Step 9: Update cli/tests/test_manifest.py imports**

Replace all `from downgrade_patcher.config import GameConfig` with `from downgrade_common.config import GameConfig`.

Replace all `from downgrade_patcher.manifest import` with `from downgrade_patcher.manifest import` (this stays the same — manifest is still in the CLI package).

- [ ] **Step 10: Update cli/tests/test_cli.py warm-cache test**

The warm-cache test needs updating since we now mock `downgrade_common.chunking.subprocess.run` instead of `downgrade_patcher.cli.subprocess.run`. Replace the `test_warm_cache_generates_patches` test:

```python
def test_warm_cache_generates_patches(tmp_path: Path, sample_games_json: Path):
    store_root = tmp_path / "store"
    runner = CliRunner()

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

    with mock_patch("downgrade_common.chunking.subprocess.run") as mock_run:
        mock_run.return_value.returncode = 0
        def side_effect(cmd, **kwargs):
            out_idx = cmd.index("-o") + 1
            Path(cmd[out_idx]).write_bytes(b"fake-patch")
            return mock_run.return_value
        mock_run.side_effect = side_effect

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
    assert mock_run.call_count >= 1
    cmd_str = " ".join(mock_run.call_args_list[0][0][0])
    assert "--patch-from" in cmd_str
```

- [ ] **Step 11: Install CLI in dev mode and run all tests**

```bash
cd /home/tbaldrid/oss/downgrade-patcher/cli && pip install -e ".[dev]"
cd /home/tbaldrid/oss/downgrade-patcher/cli && pytest tests/ -v
```

Expected: All CLI tests PASS (may need to also ensure common is installed first).

- [ ] **Step 12: Commit**

```bash
git add cli/ config/ && git rm -r src/ tests/ pyproject.toml games.json
git commit -m "refactor: move CLI into cli/ subproject, use downgrade_common"
```

---

### Task 5: Server Scaffolding and Settings

**Files:**
- Create: `server/pyproject.toml`
- Create: `server/src/patch_server/__init__.py`
- Create: `server/src/patch_server/settings.py`

- [ ] **Step 1: Create server/pyproject.toml**

```toml
[build-system]
requires = ["hatchling"]
build-backend = "hatchling.build"

[project]
name = "downgrade-patcher-server"
version = "0.1.0"
requires-python = ">=3.12"
dependencies = [
    "fastapi>=0.115",
    "uvicorn>=0.30",
    "pydantic-settings>=2.0",
    "downgrade-common",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.0",
    "httpx>=0.27",
    "pytest-asyncio>=0.24",
]

[tool.hatch.build.targets.wheel]
packages = ["src/patch_server"]
```

- [ ] **Step 2: Create package init**

```python
# server/src/patch_server/__init__.py
```

- [ ] **Step 3: Create settings module**

```python
# server/src/patch_server/settings.py
from pathlib import Path

from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    store_root: Path = Path("store")
    cache_root: Path = Path("cache")
    config_dir: Path = Path("config")

    model_config = {"env_prefix": "DOWNGRADE_"}
```

- [ ] **Step 4: Install server in dev mode**

```bash
cd /home/tbaldrid/oss/downgrade-patcher/server && pip install -e ".[dev]"
```

- [ ] **Step 5: Commit**

```bash
git add server/pyproject.toml server/src/patch_server/__init__.py server/src/patch_server/settings.py
git commit -m "scaffold: server subproject with settings"
```

---

### Task 6: Store Reader

**Files:**
- Create: `server/src/patch_server/store_reader.py`
- Create: `server/tests/test_store_reader.py`

- [ ] **Step 1: Write failing tests**

```python
# server/tests/test_store_reader.py
import json
from pathlib import Path
import pytest
from downgrade_common.config import GameConfig
from patch_server.store_reader import StoreReader


@pytest.fixture
def populated_store(tmp_path: Path) -> tuple[Path, Path]:
    """Creates a minimal store and config directory, returns (store_root, config_dir)."""
    store_root = tmp_path / "store"
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    game_config = {
        "games": [
            {
                "slug": "skyrim-se",
                "name": "Skyrim Special Edition",
                "steam_app_id": 489830,
                "depot_ids": [489833],
                "exe_path": "SkyrimSE.exe",
                "best_of_both_worlds": True,
                "nexus_domain": "skyrimspecialedition",
                "nexus_mod_id": 12345,
            }
        ]
    }
    (config_dir / "games.json").write_text(json.dumps(game_config))

    # Create a version with one file
    version_dir = store_root / "skyrim-se" / "1.5.97"
    version_dir.mkdir(parents=True)
    (version_dir / "SkyrimSE.exe").write_bytes(b"exe-content")

    # Create manifest
    manifests_dir = store_root / "skyrim-se" / "manifests"
    manifests_dir.mkdir(parents=True)
    manifest = {
        "game": "skyrim-se",
        "version": "1.5.97",
        "files": [
            {"path": "SkyrimSE.exe", "size": 11, "xxhash3": "abc123"}
        ],
    }
    (manifests_dir / "1.5.97.json").write_text(json.dumps(manifest))

    # Create manifest index
    index = {"game": "skyrim-se", "versions": {"abc123": "1.5.97"}}
    (store_root / "skyrim-se" / "manifest-index.json").write_text(json.dumps(index))

    # Create hash index
    hash_index = {"abc123": [str(version_dir / "SkyrimSE.exe")]}
    (store_root / "skyrim-se" / "hash-index.json").write_text(json.dumps(hash_index))

    return store_root, config_dir


def test_get_games(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    games = reader.get_games()
    assert len(games) == 1
    assert games[0].slug == "skyrim-se"


def test_get_manifest_index(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    index = reader.get_manifest_index("skyrim-se")
    assert index["versions"]["abc123"] == "1.5.97"


def test_get_manifest_index_unknown_game(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.get_manifest_index("nonexistent") is None


def test_get_manifest(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    manifest = reader.get_manifest("skyrim-se", "1.5.97")
    assert manifest["version"] == "1.5.97"
    assert len(manifest["files"]) == 1


def test_get_manifest_unknown_version(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.get_manifest("skyrim-se", "9.9.9") is None


def test_resolve_hash(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    path = reader.resolve_hash("skyrim-se", "abc123")
    assert path is not None
    assert path.exists()


def test_resolve_hash_unknown(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.resolve_hash("skyrim-se", "nonexistent") is None


def test_resolve_hash_wrong_game(populated_store):
    store_root, config_dir = populated_store
    reader = StoreReader(store_root, config_dir)
    assert reader.resolve_hash("fallout4", "abc123") is None
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_store_reader.py -v`
Expected: ImportError.

- [ ] **Step 3: Implement StoreReader**

```python
# server/src/patch_server/store_reader.py
import json
from pathlib import Path

from downgrade_common.config import GameConfig, load_games


class StoreReader:
    def __init__(self, store_root: Path, config_dir: Path):
        self._games = load_games(config_dir / "games.json")
        self._manifest_indexes: dict[str, dict] = {}
        self._manifests: dict[str, dict[str, dict]] = {}
        self._hash_indexes: dict[str, dict[str, list[str]]] = {}

        for game in self._games:
            slug = game.slug
            game_dir = store_root / slug

            # Load manifest index
            mi_path = game_dir / "manifest-index.json"
            if mi_path.exists():
                self._manifest_indexes[slug] = json.loads(mi_path.read_text())

            # Load hash index
            hi_path = game_dir / "hash-index.json"
            if hi_path.exists():
                self._hash_indexes[slug] = json.loads(hi_path.read_text())

            # Load all manifests
            manifests_dir = game_dir / "manifests"
            if manifests_dir.exists():
                self._manifests[slug] = {}
                for path in manifests_dir.glob("*.json"):
                    data = json.loads(path.read_text())
                    self._manifests[slug][data["version"]] = data

    def get_games(self) -> list[GameConfig]:
        return self._games

    def get_manifest_index(self, game_slug: str) -> dict | None:
        return self._manifest_indexes.get(game_slug)

    def get_manifest(self, game_slug: str, version: str) -> dict | None:
        game_manifests = self._manifests.get(game_slug)
        if game_manifests is None:
            return None
        return game_manifests.get(version)

    def resolve_hash(self, game_slug: str, file_hash: str) -> Path | None:
        game_index = self._hash_indexes.get(game_slug)
        if game_index is None:
            return None
        paths = game_index.get(file_hash)
        if not paths:
            return None
        # Return the first available path
        for p in paths:
            path = Path(p)
            if path.exists():
                return path
        return None
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_store_reader.py -v`
Expected: All 8 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add server/src/patch_server/store_reader.py server/tests/test_store_reader.py
git commit -m "feat: StoreReader with in-memory indexes loaded at startup"
```

---

### Task 7: Lazy Patch Generation with Locking

**Files:**
- Create: `server/src/patch_server/patch_gen.py`
- Create: `server/tests/test_patch_gen.py`

- [ ] **Step 1: Write failing tests**

```python
# server/tests/test_patch_gen.py
import asyncio
import json
from pathlib import Path
from unittest.mock import patch as mock_patch, AsyncMock
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
        total_chunks=1,
        chunks=[ChunkMeta(index=0, size=50)],
    )

    with mock_patch("patch_server.patch_gen.generate_chunked_patches") as mock_gen:
        mock_gen.return_value = expected_meta
        meta = await generator.ensure_patches(
            source_path=source,
            target_path=target,
            game_slug="skyrim-se",
            source_hash="aaa111",
            target_hash="bbb222",
        )

    assert meta.total_chunks == 1
    mock_gen.assert_called_once()


@pytest.mark.asyncio
async def test_ensure_patches_returns_cached(tmp_path, cache, generator):
    # Pre-populate cache
    meta = PatchMeta(
        total_chunks=1,
        chunks=[ChunkMeta(index=0, size=50)],
    )
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    with mock_patch("patch_server.patch_gen.generate_chunked_patches") as mock_gen:
        result = await generator.ensure_patches(
            source_path=tmp_path / "source.bin",
            target_path=tmp_path / "target.bin",
            game_slug="skyrim-se",
            source_hash="aaa111",
            target_hash="bbb222",
        )

    assert result.total_chunks == 1
    mock_gen.assert_not_called()


@pytest.mark.asyncio
async def test_ensure_patches_locks_concurrent_requests(tmp_path, cache, generator):
    source = tmp_path / "source.bin"
    source.write_bytes(b"source")
    target = tmp_path / "target.bin"
    target.write_bytes(b"target")

    call_count = 0

    def mock_generate(**kwargs):
        nonlocal call_count
        call_count += 1
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

    # Only one call should have happened; the second should hit cache after lock
    assert mock_gen.call_count == 1
    assert all(r.total_chunks == 1 for r in results)
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_patch_gen.py -v`
Expected: ImportError.

- [ ] **Step 3: Implement PatchGenerator**

```python
# server/src/patch_server/patch_gen.py
import asyncio
import shutil
from pathlib import Path

from downgrade_common.cache import CacheLayout, PatchMeta
from downgrade_common.chunking import generate_chunked_patches


class PatchGenerator:
    def __init__(self, cache: CacheLayout):
        self._cache = cache
        self._locks: dict[tuple[str, str], asyncio.Lock] = {}

    def _get_lock(self, source_hash: str, target_hash: str) -> asyncio.Lock:
        key = (source_hash, target_hash)
        if key not in self._locks:
            self._locks[key] = asyncio.Lock()
        return self._locks[key]

    async def ensure_patches(
        self,
        source_path: Path,
        target_path: Path,
        game_slug: str,
        source_hash: str,
        target_hash: str,
    ) -> PatchMeta:
        # Fast path: already cached
        cached = self._cache.read_meta(game_slug, source_hash, target_hash)
        if cached is not None:
            return cached

        lock = self._get_lock(source_hash, target_hash)
        async with lock:
            # Double-check after acquiring lock
            cached = self._cache.read_meta(game_slug, source_hash, target_hash)
            if cached is not None:
                return cached

            try:
                meta = await asyncio.to_thread(
                    generate_chunked_patches,
                    source_path=source_path,
                    target_path=target_path,
                    game_slug=game_slug,
                    source_hash=source_hash,
                    target_hash=target_hash,
                    cache=self._cache,
                )
                return meta
            except Exception:
                # Clean up partial cache on failure
                patch_dir = self._cache.patch_dir(game_slug, source_hash, target_hash)
                shutil.rmtree(patch_dir, ignore_errors=True)
                raise
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_patch_gen.py -v`
Expected: All 3 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add server/src/patch_server/patch_gen.py server/tests/test_patch_gen.py
git commit -m "feat: lazy patch generation with asyncio locking"
```

---

### Task 8: FastAPI App with Lifespan and Games Route

**Files:**
- Create: `server/src/patch_server/app.py`
- Create: `server/src/patch_server/routes/__init__.py`
- Create: `server/src/patch_server/routes/games.py`
- Create: `server/tests/conftest.py`
- Create: `server/tests/test_routes_games.py`

- [ ] **Step 1: Write failing test for GET /api/games**

```python
# server/tests/conftest.py
import json
from pathlib import Path
import pytest
from httpx import AsyncClient, ASGITransport
from patch_server.app import create_app


@pytest.fixture
def store_and_config(tmp_path: Path) -> tuple[Path, Path, Path]:
    """Returns (store_root, cache_root, config_dir) with minimal test data."""
    store_root = tmp_path / "store"
    cache_root = tmp_path / "cache"
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    games = {
        "games": [
            {
                "slug": "skyrim-se",
                "name": "Skyrim Special Edition",
                "steam_app_id": 489830,
                "depot_ids": [489833],
                "exe_path": "SkyrimSE.exe",
                "best_of_both_worlds": True,
                "nexus_domain": "skyrimspecialedition",
                "nexus_mod_id": 12345,
            }
        ]
    }
    (config_dir / "games.json").write_text(json.dumps(games))

    # Create version with files
    v_dir = store_root / "skyrim-se" / "1.5.97"
    v_dir.mkdir(parents=True)
    (v_dir / "SkyrimSE.exe").write_bytes(b"exe-v1-content")

    # Manifest
    manifests_dir = store_root / "skyrim-se" / "manifests"
    manifests_dir.mkdir(parents=True)
    manifest = {
        "game": "skyrim-se",
        "version": "1.5.97",
        "files": [
            {"path": "SkyrimSE.exe", "size": 14, "xxhash3": "aaa111"}
        ],
    }
    (manifests_dir / "1.5.97.json").write_text(json.dumps(manifest))

    # Manifest index
    mi = {"game": "skyrim-se", "versions": {"aaa111": "1.5.97"}}
    (store_root / "skyrim-se" / "manifest-index.json").write_text(json.dumps(mi))

    # Hash index
    hi = {"aaa111": [str(v_dir / "SkyrimSE.exe")]}
    (store_root / "skyrim-se" / "hash-index.json").write_text(json.dumps(hi))

    return store_root, cache_root, config_dir


@pytest.fixture
async def client(store_and_config) -> AsyncClient:
    store_root, cache_root, config_dir = store_and_config
    app = create_app(
        store_root=store_root,
        cache_root=cache_root,
        config_dir=config_dir,
    )
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as c:
        yield c
```

```python
# server/tests/test_routes_games.py
import pytest


@pytest.mark.asyncio
async def test_get_games(client):
    resp = await client.get("/api/games")
    assert resp.status_code == 200
    data = resp.json()
    assert len(data["games"]) == 1
    assert data["games"][0]["slug"] == "skyrim-se"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_routes_games.py -v`
Expected: ImportError.

- [ ] **Step 3: Implement routes/games.py**

```python
# server/src/patch_server/routes/__init__.py
```

```python
# server/src/patch_server/routes/games.py
from fastapi import APIRouter, Request

router = APIRouter()


@router.get("/api/games")
async def get_games(request: Request):
    store_reader = request.app.state.store_reader
    games = store_reader.get_games()
    return {"games": [g.model_dump() for g in games]}
```

- [ ] **Step 4: Implement app.py with lifespan**

```python
# server/src/patch_server/app.py
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI

from downgrade_common.cache import CacheLayout
from patch_server.store_reader import StoreReader
from patch_server.patch_gen import PatchGenerator
from patch_server.routes.games import router as games_router
from patch_server.routes.manifests import router as manifests_router
from patch_server.routes.patches import router as patches_router


def create_app(
    store_root: Path,
    cache_root: Path,
    config_dir: Path,
) -> FastAPI:
    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.store_reader = StoreReader(store_root, config_dir)
        app.state.cache = CacheLayout(cache_root)
        app.state.patch_gen = PatchGenerator(app.state.cache)
        yield

    app = FastAPI(lifespan=lifespan)
    app.include_router(games_router)
    app.include_router(manifests_router)
    app.include_router(patches_router)
    return app
```

Note: This references manifests and patches routers that don't exist yet. Create stubs:

```python
# server/src/patch_server/routes/manifests.py
from fastapi import APIRouter

router = APIRouter()
```

```python
# server/src/patch_server/routes/patches.py
from fastapi import APIRouter

router = APIRouter()
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_routes_games.py -v`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add server/src/patch_server/app.py server/src/patch_server/routes/ server/tests/conftest.py server/tests/test_routes_games.py
git commit -m "feat: FastAPI app with lifespan and GET /api/games"
```

---

### Task 9: Manifest Routes

**Files:**
- Modify: `server/src/patch_server/routes/manifests.py`
- Create: `server/tests/test_routes_manifests.py`

- [ ] **Step 1: Write failing tests**

```python
# server/tests/test_routes_manifests.py
import pytest


@pytest.mark.asyncio
async def test_get_manifest_index(client):
    resp = await client.get("/api/skyrim-se/manifest/index")
    assert resp.status_code == 200
    data = resp.json()
    assert "versions" in data
    assert data["versions"]["aaa111"] == "1.5.97"


@pytest.mark.asyncio
async def test_get_manifest_index_unknown_game(client):
    resp = await client.get("/api/nonexistent/manifest/index")
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_get_manifest(client):
    resp = await client.get("/api/skyrim-se/manifest/1.5.97")
    assert resp.status_code == 200
    data = resp.json()
    assert data["game"] == "skyrim-se"
    assert data["version"] == "1.5.97"
    assert len(data["files"]) == 1


@pytest.mark.asyncio
async def test_get_manifest_unknown_version(client):
    resp = await client.get("/api/skyrim-se/manifest/9.9.9")
    assert resp.status_code == 404
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_routes_manifests.py -v`
Expected: 404 on all (routes not implemented yet).

- [ ] **Step 3: Implement manifest routes**

```python
# server/src/patch_server/routes/manifests.py
from fastapi import APIRouter, Request, HTTPException

router = APIRouter()


@router.get("/api/{game}/manifest/index")
async def get_manifest_index(game: str, request: Request):
    store_reader = request.app.state.store_reader
    index = store_reader.get_manifest_index(game)
    if index is None:
        raise HTTPException(status_code=404, detail=f"Unknown game: {game}")
    return index


@router.get("/api/{game}/manifest/{version}")
async def get_manifest(game: str, version: str, request: Request):
    store_reader = request.app.state.store_reader
    manifest = store_reader.get_manifest(game, version)
    if manifest is None:
        raise HTTPException(status_code=404, detail=f"Manifest not found: {game} {version}")
    return manifest
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_routes_manifests.py -v`
Expected: All 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add server/src/patch_server/routes/manifests.py server/tests/test_routes_manifests.py
git commit -m "feat: manifest index and manifest endpoints"
```

---

### Task 10: Patch Routes — Meta and Chunk Endpoints

**Files:**
- Modify: `server/src/patch_server/routes/patches.py`
- Create: `server/tests/test_routes_patches.py`

- [ ] **Step 1: Write failing tests**

```python
# server/tests/test_routes_patches.py
import json
from pathlib import Path
from unittest.mock import patch as mock_patch
import pytest
from downgrade_common.cache import CacheLayout, PatchMeta, ChunkMeta


@pytest.fixture
def store_with_two_versions(tmp_path: Path) -> tuple[Path, Path, Path]:
    """Store with two versions of SkyrimSE.exe so patches can be requested."""
    store_root = tmp_path / "store"
    cache_root = tmp_path / "cache"
    config_dir = tmp_path / "config"
    config_dir.mkdir()

    games = {
        "games": [
            {
                "slug": "skyrim-se",
                "name": "Skyrim Special Edition",
                "steam_app_id": 489830,
                "depot_ids": [489833],
                "exe_path": "SkyrimSE.exe",
                "best_of_both_worlds": True,
                "nexus_domain": "skyrimspecialedition",
                "nexus_mod_id": 12345,
            }
        ]
    }
    (config_dir / "games.json").write_text(json.dumps(games))

    # Version 1
    v1_dir = store_root / "skyrim-se" / "1.5.97"
    v1_dir.mkdir(parents=True)
    (v1_dir / "SkyrimSE.exe").write_bytes(b"exe-v1-content")

    # Version 2
    v2_dir = store_root / "skyrim-se" / "1.6.1170"
    v2_dir.mkdir(parents=True)
    (v2_dir / "SkyrimSE.exe").write_bytes(b"exe-v2-content-different")

    manifests_dir = store_root / "skyrim-se" / "manifests"
    manifests_dir.mkdir(parents=True)

    m1 = {
        "game": "skyrim-se", "version": "1.5.97",
        "files": [{"path": "SkyrimSE.exe", "size": 14, "xxhash3": "aaa111"}],
    }
    m2 = {
        "game": "skyrim-se", "version": "1.6.1170",
        "files": [{"path": "SkyrimSE.exe", "size": 24, "xxhash3": "bbb222"}],
    }
    (manifests_dir / "1.5.97.json").write_text(json.dumps(m1))
    (manifests_dir / "1.6.1170.json").write_text(json.dumps(m2))

    mi = {"game": "skyrim-se", "versions": {"aaa111": "1.5.97", "bbb222": "1.6.1170"}}
    (store_root / "skyrim-se" / "manifest-index.json").write_text(json.dumps(mi))

    hi = {
        "aaa111": [str(v1_dir / "SkyrimSE.exe")],
        "bbb222": [str(v2_dir / "SkyrimSE.exe")],
    }
    (store_root / "skyrim-se" / "hash-index.json").write_text(json.dumps(hi))

    return store_root, cache_root, config_dir


@pytest.fixture
async def patch_client(store_with_two_versions):
    from httpx import AsyncClient, ASGITransport
    from patch_server.app import create_app

    store_root, cache_root, config_dir = store_with_two_versions
    app = create_app(store_root=store_root, cache_root=cache_root, config_dir=config_dir)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as c:
        yield c, cache_root


@pytest.mark.asyncio
async def test_get_patch_meta_cached(patch_client):
    client, cache_root = patch_client
    cache = CacheLayout(cache_root)
    meta = PatchMeta(total_chunks=2, chunks=[ChunkMeta(index=0, size=100), ChunkMeta(index=1, size=50)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    resp = await client.get("/api/skyrim-se/patch/aaa111/bbb222/meta")
    assert resp.status_code == 200
    data = resp.json()
    assert data["total_chunks"] == 2


@pytest.mark.asyncio
async def test_get_patch_chunk_cached(patch_client):
    client, cache_root = patch_client
    cache = CacheLayout(cache_root)
    meta = PatchMeta(total_chunks=1, chunks=[ChunkMeta(index=0, size=10)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)
    cache.chunk_path("skyrim-se", "aaa111", "bbb222", 0).write_bytes(b"patch-data")

    resp = await client.get("/api/skyrim-se/patch/aaa111/bbb222/0")
    assert resp.status_code == 200
    assert resp.content == b"patch-data"
    assert resp.headers["cache-control"] == "public, max-age=31536000, immutable"


@pytest.mark.asyncio
async def test_get_patch_chunk_out_of_range(patch_client):
    client, cache_root = patch_client
    cache = CacheLayout(cache_root)
    meta = PatchMeta(total_chunks=1, chunks=[ChunkMeta(index=0, size=10)])
    cache.write_meta("skyrim-se", "aaa111", "bbb222", meta)

    resp = await client.get("/api/skyrim-se/patch/aaa111/bbb222/5")
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_get_patch_unknown_hash(patch_client):
    client, _ = patch_client
    resp = await client.get("/api/skyrim-se/patch/nonexistent/bbb222/meta")
    assert resp.status_code == 404


@pytest.mark.asyncio
async def test_get_patch_cross_game_rejected(patch_client):
    client, _ = patch_client
    # aaa111 belongs to skyrim-se, not fallout4
    resp = await client.get("/api/fallout4/patch/aaa111/bbb222/meta")
    assert resp.status_code == 404
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_routes_patches.py -v`
Expected: 404 on all or errors.

- [ ] **Step 3: Implement patch routes**

```python
# server/src/patch_server/routes/patches.py
from fastapi import APIRouter, Request, HTTPException
from fastapi.responses import FileResponse, Response

router = APIRouter()

CACHE_HEADERS = {"Cache-Control": "public, max-age=31536000, immutable"}


@router.get("/api/{game}/patch/{source_hash}/{target_hash}/meta")
async def get_patch_meta(game: str, source_hash: str, target_hash: str, request: Request):
    store_reader = request.app.state.store_reader
    cache = request.app.state.cache
    patch_gen = request.app.state.patch_gen

    # TOS: verify both hashes belong to this game
    source_path = store_reader.resolve_hash(game, source_hash)
    target_path = store_reader.resolve_hash(game, target_hash)
    if source_path is None or target_path is None:
        raise HTTPException(status_code=404, detail="Hash not found for this game")

    meta = await patch_gen.ensure_patches(
        source_path=source_path,
        target_path=target_path,
        game_slug=game,
        source_hash=source_hash,
        target_hash=target_hash,
    )
    return Response(
        content=meta.model_dump_json(),
        media_type="application/json",
        headers=CACHE_HEADERS,
    )


@router.get("/api/{game}/patch/{source_hash}/{target_hash}/{chunk_index}")
async def get_patch_chunk(
    game: str,
    source_hash: str,
    target_hash: str,
    chunk_index: int,
    request: Request,
):
    store_reader = request.app.state.store_reader
    cache = request.app.state.cache
    patch_gen = request.app.state.patch_gen

    # TOS: verify both hashes belong to this game
    source_path = store_reader.resolve_hash(game, source_hash)
    target_path = store_reader.resolve_hash(game, target_hash)
    if source_path is None or target_path is None:
        raise HTTPException(status_code=404, detail="Hash not found for this game")

    # Ensure patches are generated
    meta = await patch_gen.ensure_patches(
        source_path=source_path,
        target_path=target_path,
        game_slug=game,
        source_hash=source_hash,
        target_hash=target_hash,
    )

    if chunk_index < 0 or chunk_index >= meta.total_chunks:
        raise HTTPException(status_code=404, detail="Chunk index out of range")

    chunk_path = cache.chunk_path(game, source_hash, target_hash, chunk_index)
    if not chunk_path.exists():
        raise HTTPException(status_code=500, detail="Chunk file missing from cache")

    return FileResponse(
        path=chunk_path,
        media_type="application/octet-stream",
        headers=CACHE_HEADERS,
    )
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/test_routes_patches.py -v`
Expected: All 5 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add server/src/patch_server/routes/patches.py server/tests/test_routes_patches.py
git commit -m "feat: patch meta and chunk endpoints with TOS enforcement"
```

---

### Task 11: Run Full Server Test Suite

**Files:** None (verification only)

- [ ] **Step 1: Run all common tests**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/common && pytest tests/ -v`
Expected: All tests PASS.

- [ ] **Step 2: Run all server tests**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/server && pytest tests/ -v`
Expected: All tests PASS.

- [ ] **Step 3: Run all CLI tests**

Run: `cd /home/tbaldrid/oss/downgrade-patcher/cli && pytest tests/ -v`
Expected: All tests PASS.

- [ ] **Step 4: Verify server starts**

Run: `cd /home/tbaldrid/oss/downgrade-patcher && DOWNGRADE_STORE_ROOT=store DOWNGRADE_CACHE_ROOT=cache DOWNGRADE_CONFIG_DIR=config python -c "from patch_server.app import create_app; print('OK')"`
Expected: Prints "OK".

- [ ] **Step 5: Commit any fixes if needed**
