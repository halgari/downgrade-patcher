# Downgrade Patcher — System Design

## Overview

A game-agnostic downgrade patching system that lets users revert Bethesda games (Skyrim SE, Fallout 4, Starfield, Oblivion Remastered) to previous versions. The system consists of a Qt/C++ desktop client, a Python/FastAPI patch service, and Python CLI tooling for managing game versions.

Patches are generated using zstd's native `--patch-from` capability. Patches are built lazily on first request, cached on disk, and served as fixed-size chunks through Cloudflare for CDN cacheability.

## Components

### 1. Desktop Client (Qt/C++)

A single application that works for all supported games. Distributed via Nexus Mods with auto-update through the Nexus upload API.

**Startup flow:**

1. Auto-detect installed games by checking Steam library paths (via `libraryfolders.vdf`)
2. For each detected game, hash the exe with xxhash3 and fetch the manifest index from the server to identify the installed version
3. Present the user with: detected game, current version, and a dropdown of available target versions

**Patching flow:**

1. User selects a target version
2. Client fetches the target version's manifest
3. Client hashes local files, compares against target manifest to build a diff list
4. For each file that differs: download patch chunks from the server, apply with zstd, verify result xxhash3 matches target manifest
5. Files absent from the target get deleted; files that are new in the target (no matching hash in source) are patched from the closest file by name in the source version — this produces a less efficient patch but avoids distributing raw game files. The server resolves "closest by name" during patch generation by finding the best filename match within the same game's version store.

**Best-of-both-worlds mode:**

- Hidden behind a settings toggle, off by default
- When enabled, shows two version selectors: one for program files (exe, DLLs), one for data files (BSAs, ESPs, ESMs)
- Patching runs the same flow twice with different file sets against different target manifests

**Progress UI:**

- Per-file progress (chunk download + apply)
- Overall progress bar
- No pause/resume initially — chunks are small enough that resuming means re-downloading the current chunk at worst

**Auto-update:**

- On launch, check for newer client builds (via Nexus API or a version endpoint on the patch server)
- Prompt user to download from Nexus if an update is available

### 2. Patch Service (Python/FastAPI)

A web API that serves manifests and chunked patches, with Cloudflare in front.

**API endpoints:**

- `GET /api/{game}/manifest/index` — exe-hash to version mapping
- `GET /api/{game}/manifest/{version}` — full manifest for a version
- `GET /api/{game}/patch/{source-hash}/{target-hash}/chunk/{n}` — chunk N of the zstd patch between two file versions
- `GET /api/{game}/file/{target-hash}/chunk/{n}` — chunk N of a zstd patch for a file new in the target version, patched from the closest file by name in the source version. The client provides the source version as a query parameter so the server can find the best donor file.

**Lazy patch generation:**

1. Client requests chunk 0 of a patch that doesn't exist in cache
2. Server acquires a lock for the (source-hash, target-hash) pair to prevent duplicate generation
3. Server runs `zstd --patch-from=<source-file> <target-file> -o <patch-file>`
4. Splits the patch into 8MB chunks on disk
5. Returns the requested chunk
6. Concurrent requests for other chunks of the same patch wait on the lock, then serve from cache

**Caching:**

- Patches cached on disk at `cache/{game-slug}/{source-xxhash3}/{target-xxhash3}/chunk-{n}`
- A `meta.json` alongside the chunks stores total chunk count and individual chunk sizes
- `Cache-Control` headers set for aggressive Cloudflare caching
- No automatic cache eviction initially — disk is cheap and the working set is bounded

### 3. Version Store

Raw game files organized on the server filesystem:

```
store/
  skyrim-se/
    1.5.97/
      SkyrimSE.exe
      Data/
        Skyrim - Textures0.bsa
        ...
    1.6.1170/
      ...
```

Populated via the CLI tooling. Each version directory is a complete copy of the game files from that depot download.

**Hash-to-file index:** During ingest, the CLI builds a reverse lookup mapping xxhash3 → file path(s) in the store. This allows the patch service to resolve a hash from a patch request URL to the actual file on disk. Stored as `store/{game-slug}/hash-index.json`.

### 4. CLI Tooling (Python)

A command-line tool (`downgrade-tool`) for server administration.

**Commands:**

- `downgrade-tool download --game skyrim-se --depot <depot-id> --manifest <steam-manifest-id>`
  Wraps DepotDownloader to download a specific depot version to a staging directory.

- `downgrade-tool ingest --game skyrim-se --version 1.6.1180 --depot-path ./staging/`
  Copies files into the version store, hashes all files with xxhash3, generates the manifest, and updates the manifest index.

- `downgrade-tool list-versions --game skyrim-se`
  Lists all known versions for a game.

- `downgrade-tool warm-cache --game skyrim-se --from 1.6.1180 --to 1.5.97`
  Pre-generates patches for a specific version transition to avoid cold-start latency for the first user.

- `downgrade-tool publish-client --version 2.1.0`
  Uploads a new client build to Nexus Mods via their upload API.

## Data Model

### Game Configuration

Stored on the server, fetched by the client at startup:

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
    },
    {
      "slug": "fallout4",
      "name": "Fallout 4",
      "steam_app_id": 377160,
      "depot_ids": [377162],
      "exe_path": "Fallout4.exe",
      "best_of_both_worlds": false,
      "nexus_domain": "fallout4",
      "nexus_mod_id": 67890
    }
  ]
}
```

Adding a new game: add an entry to this config, download and ingest versions with the CLI. The client picks it up automatically on next manifest fetch.

### Manifest

One manifest per game version:

```json
{
  "game": "skyrim-se",
  "version": "1.6.1170",
  "files": [
    {
      "path": "SkyrimSE.exe",
      "size": 75210240,
      "xxhash3": "a1b2c3d4e5f6"
    },
    {
      "path": "Data/Skyrim - Textures0.bsa",
      "size": 3284019200,
      "xxhash3": "f6e5d4c3b2a1"
    }
  ]
}
```

### Manifest Index

Maps exe hashes to versions for auto-detection:

```json
{
  "game": "skyrim-se",
  "versions": {
    "a1b2c3d4e5f6": "1.5.97",
    "d4e5f6a1b2c3": "1.6.1170"
  }
}
```

### Patch Cache

```
cache/
  skyrim-se/
    {source-xxhash3}/
      {target-xxhash3}/
        meta.json
        chunk-0
        chunk-1
        ...
```

`meta.json` contains total chunk count and individual chunk sizes so the client knows what to expect.

## Multi-Game Isolation

Each game is fully isolated by its slug in every path and URL:

- Version store: `store/{slug}/...`
- Patch cache: `cache/{slug}/...`
- API routes: `/api/{slug}/...`

No cross-game operations are possible by construction. The game slug is baked into every storage path and API URL.

**TOS enforcement:** The patch service must verify that both the source and target hashes in a patch request belong to the same game. The hash-to-file index is per-game, so the server rejects any patch request where either hash is not found in that game's index. This prevents generating patches that could be used to reconstruct one game's files from another game's files, which would violate distribution terms.

## Release Pipeline

Manual process triggered when a new game version drops:

1. **Download:** `downgrade-tool download --game skyrim-se --depot 489833 --manifest <id>`
2. **Ingest:** `downgrade-tool ingest --game skyrim-se --version 1.6.1180 --depot-path ./staging/`
3. **Warm cache (optional):** `downgrade-tool warm-cache --game skyrim-se --from 1.6.1180 --to 1.5.97`
4. **Publish client (if needed):** `downgrade-tool publish-client --version 2.1.0`

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Desktop client | C++, Qt |
| Patch service | Python, FastAPI |
| CLI tooling | Python |
| Patch generation | zstd (`--patch-from`) |
| File hashing | xxhash3 |
| CDN | Cloudflare |
| Depot downloading | DepotDownloader |
| Client distribution | Nexus Mods |

## Subsystem Build Order

These are the recommended subsystems to implement in order, each getting its own implementation plan:

1. **CLI tooling + version store + manifest generation** — foundation everything else depends on
2. **Patch service (FastAPI)** — can test with CLI-generated data
3. **Desktop client** — consumes the running service
4. **Nexus publishing integration** — polish step once the core works
