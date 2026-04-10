# Desktop Client — System Design

## Overview

A Qt 6 Widgets desktop application (C++20, clang) that lets users downgrade Bethesda games to previous versions. Detects installed games via Steam, scans local files against server manifests, downloads chunked zstd patches from the patch service, and applies them locally. Distributed via Nexus Mods.

This is Subsystem 3 of the Downgrade Patcher. It consumes the API provided by Subsystem 2 (Patch Service).

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C++20 |
| Compiler | clang |
| UI framework | Qt 6 Widgets |
| Build system | xmake |
| HTTP client | QNetworkAccessManager |
| Patch decompression | libzstd (C API, dictionary decompression) |
| File hashing | libxxhash (xxhash3_64) |
| Auto-update check | Nexus Mods API v1 |

## Project Structure

```
client/
  xmake.lua
  src/
    main.cpp
    api/
      ApiClient.h/cpp          # async HTTP client for patch service
      types.h                  # GameConfig, Manifest, FileEntry, PatchMeta structs
    engine/
      SteamDetector.h/cpp      # find Steam + installed games
      GameScanner.h/cpp        # hash local files, compare against manifest
      HashCache.h/cpp          # persistent hash cache (size+mtime based)
      Patcher.h/cpp            # download chunks, apply zstd patches, verify
    ui/
      MainWindow.h/cpp         # top-level window, manages state transitions
      GameListWidget.h/cpp     # shows detected games + version info
      PatchWidget.h/cpp        # version selector, scan results, patch button, progress
      SettingsDialog.h/cpp     # best-of-both-worlds toggle, manual game path
  tests/
    test_main.cpp
    test_steam_detector.cpp
    test_game_scanner.cpp
    test_hash_cache.cpp
    test_patcher.cpp
    test_api_client.cpp
```

## Architecture

Hybrid layered: Qt types used throughout, separated into three layers with clear responsibilities.

- **`api/`** — talks to the patch service, returns typed structs via signals
- **`engine/`** — Steam detection, file scanning, patching logic
- **`ui/`** — Qt Widgets, thin shell over engine

All layers use Qt types (QString, QByteArray, etc.) naturally. Tests require `QCoreApplication` but no GUI.

## API Client

`ApiClient` wraps `QNetworkAccessManager`. All methods are async, results delivered via signals.

**Methods and signals:**

- `fetchGames()` → `gamesReady(QList<GameConfig>)`
- `fetchManifestIndex(gameSlug)` → `manifestIndexReady(gameSlug, QMap<QString, QString>)` (hash → version)
- `fetchManifest(gameSlug, version)` → `manifestReady(gameSlug, Manifest)`
- `fetchPatchMeta(gameSlug, sourceHash, targetHash)` → `patchMetaReady(PatchMeta)`
- `fetchPatchChunk(gameSlug, sourceHash, targetHash, chunkIndex)` → `patchChunkReady(chunkIndex, QByteArray)`
- All errors → `errorOccurred(QString)`

Server base URL is configurable, defaults to the production server.

## Steam Detection

`SteamDetector` finds installed games across platforms.

**Detection strategy (in order):**

1. Check default Steam paths:
   - Windows: `C:\Program Files (x86)\Steam\steamapps\`
   - Linux: `~/.steam/steam/steamapps/` and `~/.local/share/Steam/steamapps/`
2. On Windows: read registry at `HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Valve\Steam` for custom install path
3. Parse `libraryfolders.vdf` to find all Steam library folders
4. For each library folder: check `steamapps/appmanifest_<appid>.acf` for installed games
5. Match against known app IDs from the game config

**Manual override:** Settings dialog lets the user point to a game directory manually. Stored in app settings (QSettings).

**Output:** `QList<DetectedGame>` where `DetectedGame { gameSlug, installPath }`.

## Game Scanning

`GameScanner` hashes local files and compares against a target manifest.

**Hash cache (`HashCache`):**
- Persisted to `<game-install-path>/.downgrade-patcher-cache.json`
- Stores per file: `{ path, size, mtime, xxhash3 }`
- On scan: if size + mtime match cached entry, use cached hash (skip rehashing)
- Invalidated on any size or mtime change

**File filtering:** Only hash files that appear in any known manifest for that game. Build a union of all known file paths across all versions. Files outside this set (mods, generated content) are ignored entirely.

**Progress:** Emits `fileHashProgress(currentFile, filesCompleted, totalFiles)` so the UI shows a scanning progress bar.

**Scan result categories:**

| Category | Meaning | Action |
|----------|---------|--------|
| `unchanged` | Hash matches target | Skip |
| `patchable` | Hash differs, source hash known to server | Will be patched |
| `unknown` | Local hash not in any manifest (modded/unrecognized) | May fail (server 404) |
| `missing` | File in target but not on disk | Patched from closest donor by name |
| `extra` | File on disk, not in target | Will be deleted |

The user sees this breakdown before patching starts.

## Patcher Engine

`Patcher` applies patches for a game transition based on a `ScanResult`.

### Patching Flow

**For `patchable` files:**

1. Fetch patch meta: `GET /api/{game}/patch/{sourceHash}/{targetHash}/meta`
2. Download chunks in parallel (configurable concurrency, default 4 simultaneous)
3. For each completed chunk: decompress using libzstd with the local source file as dictionary
4. Write decompressed data to correct offset in a temp file (`<filename>.tmp`)
5. Chunks can arrive out of order — track completion, write to correct position
6. After all chunks complete: verify assembled file's xxhash3 matches target manifest
7. Atomic replace: rename temp file over the original

**For `missing` files (new in target):**

1. Client picks the closest file by name from local files as the donor
2. Sends donor hash as source hash to the server
3. Same parallel chunk download + decompress flow
4. Write to the new file path

**For `extra` files (not in target):**

1. Delete the file

**Skip:** `unchanged` and `unknown` files.

### Parallel Chunk Downloads

`QNetworkAccessManager` handles multiple concurrent requests natively. The patcher fires up to N chunk requests at once per file (default N=4). As each chunk completes, it's decompressed and written immediately. This saturates the network while keeping memory usage bounded (N * 8MB worst case = 32MB).

### Error Handling

- Per-file errors are non-fatal — log and continue with the next file
- Temp files are cleaned up on failure (no corrupted originals)
- User sees a summary at the end: success count, fail count, list of failed files with error messages
- If hash verification fails after assembly, the temp file is deleted and the original is preserved

### Temp File Safety

All writes go to `<filename>.tmp`. Only renamed to the real path after xxhash3 verification passes. If the process crashes mid-patch, no original game files are corrupted. On startup, the scanner detects any stale `.tmp` files and cleans them up.

## UI Design

Single window, three states. No separate dialogs except settings.

### State 1: Game List

- Lists all games from server config
- Detected games show: name, detected version (or "unknown version"), install path
- Uninstalled games shown grayed out
- "Select →" button on detected games
- Footer: "Settings" and "Add game manually" links
- Auto-update banner at top if newer client available ("Update available (v2.1.0) — Download from Nexus")

### State 2: Version Selection + Scan

- Header with game name and "← Back" to return to game list
- Current version display (with warning badge if unknown)
- Target version dropdown (populated from manifest index)
- Scan results panel showing file breakdown (unchanged, patchable, unknown, missing, extra)
- "Start Patching" button
- Scan runs automatically when target version is selected

### State 3: Patching Progress

- Header with game name and target version
- Overall progress bar (files completed / total files)
- Per-file progress bar (chunks downloaded / total chunks)
- Scrolling log showing completed, in-progress, and failed files
- Non-interactive during patching (back button disabled)
- On completion: summary with success/fail counts, "Done" button returns to game list

### Best-of-Both-Worlds Mode

- Hidden behind a toggle in Settings dialog, off by default
- When enabled, State 2 shows two version dropdowns: one for program files (exe, DLLs), one for data files (BSAs, ESPs, ESMs)
- Patching runs the same flow twice with different file sets against different target manifests

## Auto-Update

On launch, the client checks for a newer version via the Nexus Mods API:

- `GET https://api.nexusmods.com/v1/games/{nexus_domain}/mods/{nexus_mod_id}/files.json`
- Ships with a read-only API key for rate limiting (standard for Nexus tools)
- Compare latest file version against client's built-in version string
- If newer: non-blocking banner at top of game list with link
- Clicking opens the Nexus mod page in the default browser
- No in-app download/install of updates

## Dependencies (xmake)

| Package | Purpose |
|---------|---------|
| qt6 (widgets, network) | UI framework, HTTP client |
| libzstd | Patch decompression with dictionary |
| libxxhash | File hashing (xxhash3_64) |
| QJsonDocument (built-in) | JSON parsing for API responses and cache files |
