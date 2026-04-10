# Patch Service — System Design

## Overview

A FastAPI web service that serves game manifests and lazily generates chunked zstd patches between game file versions. Sits behind Cloudflare for CDN caching. Part of a monorepo alongside the CLI tooling and a shared common library.

This is Subsystem 2 of the Downgrade Patcher. It depends on the version store and manifests created by Subsystem 1 (CLI tooling).

## Monorepo Structure

```
downgrade-patcher/
  common/                    # downgrade_common — shared library
    pyproject.toml
    src/downgrade_common/
      __init__.py
      config.py              # GameConfig, load_games, find_game
      hashing.py             # hash_file (xxhash3)
      chunking.py            # chunk target files, per-chunk zstd patch generation
      cache.py               # cache path layout, meta.json read/write
  cli/                       # CLI tooling (depends on downgrade_common)
    pyproject.toml
    src/downgrade_patcher/
      ...
  server/                    # FastAPI patch service (depends on downgrade_common)
    pyproject.toml
    src/patch_server/
      __init__.py
      app.py                 # FastAPI app, lifespan, config
      routes/
        __init__.py
        games.py             # GET /api/games
        manifests.py         # manifest endpoints
        patches.py           # patch chunk endpoints
      patch_gen.py           # lazy patch generation with locking
      store_reader.py        # read-only access to version store + hash index
  config/                    # shared config files
    games.json
  store/                     # version store data (populated by CLI)
  cache/                     # patch cache (written by CLI warm-cache + server)
```

### Shared Library Refactor

Code currently in the CLI package moves into `downgrade_common`:

- `config.py` — `GameConfig` model, `load_games()`, `find_game()` (unchanged)
- `hashing.py` — `hash_file()` (unchanged)

New modules in `downgrade_common`:

- `chunking.py` — splits a target file into 8MB chunks, generates per-chunk zstd patches using the whole source file as dictionary
- `cache.py` — cache path helpers and meta.json read/write

The CLI's `warm-cache` command gets refactored to use `downgrade_common.chunking` instead of its inline `_split_into_chunks`.

## API Endpoints

```
GET  /api/games                                          — game config list
GET  /api/{game}/manifest/index                          — exe-hash → version mapping
GET  /api/{game}/manifest/{version}                      — full manifest for a version
GET  /api/{game}/patch/{source-hash}/{target-hash}/meta  — chunk count and sizes
GET  /api/{game}/patch/{source-hash}/{target-hash}/{n}   — patch chunk N
```

### Client Flow

1. `GET /api/games` — discover supported games
2. `GET /api/{game}/manifest/index` — hash local exe, identify installed version
3. `GET /api/{game}/manifest/{version}` — fetch target version manifest
4. Diff local files against target manifest
5. For each changed file: `GET .../meta` to learn chunk count, then download chunks 0..N
6. Apply each chunk using the local source file as zstd dictionary
7. Assemble target file from decompressed chunks, verify xxhash3

### Cache Headers

All chunk and meta responses: `Cache-Control: public, max-age=31536000, immutable`

Content-addressable URLs (keyed by file hash) never change, so aggressive caching is safe.

### TOS Enforcement

On every patch request, the server verifies both source-hash and target-hash exist in the requested game's hash index. Returns 404 if either hash is not found. Since hash indexes are per-game, this prevents cross-game patch generation.

## Chunk-Based Patch Generation

### How It Works

Target files are split into 8MB chunks. Each chunk gets its own independent zstd patch using the **whole source file** as the `--patch-from` dictionary.

For a 3GB BSA file: ~384 chunks, each producing a small zstd patch.

**Why this approach:**
- Each patch generation is fast (small target input, bounded memory)
- Each patch chunk is small — great for CDN caching
- Client can start applying before all chunks are downloaded
- 8MB working set on the client keeps memory usage low
- The whole source file as dictionary gives zstd full context for good compression regardless of chunk size

**Handling size differences:** Source and target files may differ in size. Target chunks past the source file's length still work — zstd handles mismatched source/target sizes. If the target is shorter, there are simply fewer chunks.

### Generation Steps

1. Resolve source-hash and target-hash to file paths via hash index
2. Read the target file, split into 8MB chunks in a temp directory
3. For each target chunk: `zstd --patch-from=<whole-source-file> <target-chunk-N> -o <patch-N>`
4. Move completed patches into cache directory
5. Write `meta.json`

### Cache Layout

```
cache/
  skyrim-se/
    {source-hash}/
      {target-hash}/
        meta.json       # {"total_chunks": 3, "chunks": [{"index": 0, "size": 8142}, ...]}
                        # size = bytes of the compressed patch chunk file (for progress/allocation)
        0               # zstd patch for target chunk 0
        1               # zstd patch for target chunk 1
        2               # zstd patch for target chunk 2
```

## Server Architecture

### Startup (Lifespan)

On startup, the server loads everything into memory:

- Game config from `config/games.json`
- For each game: manifest index, hash index, all manifests

No file I/O on the hot path for reads. The only disk access during requests is reading/writing patch cache and reading source game files during patch generation.

Server gets restarted when new versions are ingested — no hot-reload needed.

### Store Reader (`store_reader.py`)

Read-only interface to the version store, backed by in-memory data loaded at startup:

- `resolve_hash(game_slug, file_hash) -> Path | None` — hash index lookup
- `get_manifest_index(game_slug) -> dict` — cached manifest index
- `get_manifest(game_slug, version) -> dict | None` — cached manifest
- `get_games() -> list[GameConfig]` — all game configs

### Lazy Patch Generation (`patch_gen.py`)

When a client requests a patch that isn't cached:

1. Acquire an in-memory `asyncio.Lock` keyed by `(source-hash, target-hash)`
2. Double-check cache (another request may have completed while waiting)
3. Run chunk-based patch generation via `asyncio.to_thread` (CPU-bound zstd work)
4. Release lock

Concurrent requests for the same patch pair wait on the lock. Requests for different pairs proceed independently.

If generation fails, the partial cache directory is deleted and the lock is released. No retry logic — the client can retry the request.

### Configuration

Server reads three filesystem paths from environment or config:

- `STORE_ROOT` — path to the version store
- `CACHE_ROOT` — path to the patch cache
- `CONFIG_DIR` — path to shared config (games.json)

## Error Handling

| Condition | Response |
|-----------|----------|
| Unknown game slug | 404 |
| Hash not in game's hash index | 404 |
| Manifest version not found | 404 |
| Chunk index >= total_chunks | 404 |
| zstd fails during generation | 500, partial cache cleaned up |
| Source file missing from disk | 500 |

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Web framework | FastAPI |
| ASGI server | uvicorn |
| Patch generation | zstd (`--patch-from`) |
| File hashing | xxhash3 |
| CDN | Cloudflare |
| Locking | asyncio.Lock (in-memory) |
| Data validation | pydantic |
