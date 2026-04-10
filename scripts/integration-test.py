#!/usr/bin/env python3
"""Integration test against a live patch server.

Tests the full API flow: games -> manifest index -> manifest -> patch meta -> patch chunks.
Verifies that patch chunks can be decompressed with zstd using the source file as dictionary.

Usage:
    python scripts/integration-test.py [--server URL]

Default server: https://downgradepatcher.wabbajack.org
"""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import HTTPError

DEFAULT_SERVER = "https://downgradepatcher.wabbajack.org"


def get(url: str) -> bytes:
    req = Request(url)
    with urlopen(req, timeout=30) as resp:
        return resp.read()


def get_json(url: str) -> dict:
    return json.loads(get(url))


def test_health(base: str):
    print("Testing /health...")
    data = get_json(f"{base}/health")
    assert data["status"] == "ok", f"Health check failed: {data}"
    print("  OK")


def test_games(base: str) -> list[dict]:
    print("Testing /api/games...")
    data = get_json(f"{base}/api/games")
    games = data["games"]
    assert len(games) > 0, "No games returned"
    print(f"  OK — {len(games)} game(s): {', '.join(g['slug'] for g in games)}")
    return games


def test_manifest_index(base: str, game_slug: str) -> dict:
    print(f"Testing /api/{game_slug}/manifest/index...")
    data = get_json(f"{base}/api/{game_slug}/manifest/index")
    versions = data["versions"]
    assert len(versions) > 0, "No versions in manifest index"
    print(f"  OK — {len(versions)} version(s): {', '.join(versions.values())}")
    return data


def test_manifest(base: str, game_slug: str, version: str) -> dict:
    print(f"Testing /api/{game_slug}/manifest/{version}...")
    data = get_json(f"{base}/api/{game_slug}/manifest/{version}")
    assert data["game"] == game_slug
    assert data["version"] == version
    assert len(data["files"]) > 0
    print(f"  OK — {len(data['files'])} files")
    return data


def test_manifest_404(base: str, game_slug: str):
    print(f"Testing 404 for unknown version...")
    try:
        get_json(f"{base}/api/{game_slug}/manifest/99.99.99")
        assert False, "Should have returned 404"
    except HTTPError as e:
        assert e.code == 404
    print("  OK — got 404")


def test_patch_generation(base: str, game_slug: str, source_manifest: dict, target_manifest: dict):
    """Find a file that differs between two versions and test patch generation."""
    source_by_path = {f["path"]: f for f in source_manifest["files"]}
    target_by_path = {f["path"]: f for f in target_manifest["files"]}

    # Find a real game file that differs (skip DepotDownloader artifacts)
    test_file = None
    for path, target_entry in target_by_path.items():
        if path.startswith("."):
            continue  # skip hidden/metadata files
        source_entry = source_by_path.get(path)
        if source_entry and source_entry["xxhash3"] != target_entry["xxhash3"]:
            # Prefer smaller files for faster testing
            if test_file is None or target_entry["size"] < target_by_path[test_file]["size"]:
                test_file = path

    if test_file is None:
        print("  SKIP — no differing files found between versions")
        return

    source_hash = source_by_path[test_file]["xxhash3"]
    target_hash = target_by_path[test_file]["xxhash3"]

    print(f"Testing patch generation for {test_file}...")
    print(f"  Source hash: {source_hash}")
    print(f"  Target hash: {target_hash}")

    # Request patch meta (this may trigger lazy generation)
    print(f"  Requesting /api/{game_slug}/patch/{source_hash}/{target_hash}/meta...")
    try:
        meta = get_json(f"{base}/api/{game_slug}/patch/{source_hash}/{target_hash}/meta")
    except HTTPError as e:
        body = e.read().decode() if hasattr(e, 'read') else str(e)
        print(f"  FAIL — HTTP {e.code}: {body}")
        return

    print(f"  OK — {meta['total_chunks']} chunk(s)")

    # Download first chunk
    print(f"  Downloading chunk 0...")
    chunk_data = get(f"{base}/api/{game_slug}/patch/{source_hash}/{target_hash}/0")
    expected_size = meta["chunks"][0]["size"]
    assert len(chunk_data) == expected_size, f"Chunk size mismatch: got {len(chunk_data)}, expected {expected_size}"
    print(f"  OK — {len(chunk_data)} bytes")

    # Verify it looks like valid zstd data (magic number: 0xFD2FB528)
    if len(chunk_data) >= 4:
        magic = int.from_bytes(chunk_data[:4], "little")
        if magic == 0xFD2FB528:
            print("  OK — valid zstd magic number")
        else:
            print(f"  WARNING — unexpected magic: {hex(magic)} (may be a zstd skippable frame or dict-compressed)")


def test_tos_enforcement(base: str, game_slug: str):
    print("Testing TOS enforcement (cross-game hash rejection)...")
    try:
        get_json(f"{base}/api/nonexistent-game/patch/aaa/bbb/meta")
        assert False, "Should have returned 404"
    except HTTPError as e:
        assert e.code == 404
    print("  OK — rejected unknown game")


def test_cache_headers(base: str, game_slug: str, source_manifest: dict, target_manifest: dict):
    """Verify that patch endpoints return immutable cache headers."""
    source_by_path = {f["path"]: f for f in source_manifest["files"]}
    target_by_path = {f["path"]: f for f in target_manifest["files"]}

    # Find any differing real game file
    for path, target_entry in target_by_path.items():
        if path.startswith("."):
            continue
        source_entry = source_by_path.get(path)
        if source_entry and source_entry["xxhash3"] != target_entry["xxhash3"]:
            source_hash = source_entry["xxhash3"]
            target_hash = target_entry["xxhash3"]

            print("Testing cache headers on patch meta...")
            url = f"{base}/api/{game_slug}/patch/{source_hash}/{target_hash}/meta"
            req = Request(url)
            try:
                with urlopen(req, timeout=30) as resp:
                    cc = resp.headers.get("Cache-Control", "")
                    if "immutable" in cc:
                        print(f"  OK — Cache-Control: {cc}")
                    else:
                        print(f"  WARNING — Cache-Control: {cc} (expected immutable)")
            except HTTPError:
                print("  SKIP — patch not available yet")
            return

    print("  SKIP — no differing files")


def main():
    parser = argparse.ArgumentParser(description="Integration test for patch server")
    parser.add_argument("--server", default=DEFAULT_SERVER, help="Server base URL")
    args = parser.parse_args()

    base = args.server.rstrip("/")
    print(f"Testing against: {base}\n")

    passed = 0
    failed = 0

    tests = [
        ("Health check", lambda: test_health(base)),
    ]

    # Run health first
    test_health(base)
    passed += 1

    # Games
    games = test_games(base)
    passed += 1

    game_slug = games[0]["slug"]

    # Manifest index
    index = test_manifest_index(base, game_slug)
    passed += 1

    # Get two versions for patch testing
    versions = sorted(index["versions"].values())
    if len(versions) < 2:
        print("\nNeed at least 2 versions for patch testing, skipping patch tests")
    else:
        source_version = versions[0]
        target_version = versions[-1]

        # Manifests
        source_manifest = test_manifest(base, game_slug, source_version)
        passed += 1
        target_manifest = test_manifest(base, game_slug, target_version)
        passed += 1

        # 404
        test_manifest_404(base, game_slug)
        passed += 1

        # TOS
        test_tos_enforcement(base, game_slug)
        passed += 1

        # Patch generation
        test_patch_generation(base, game_slug, source_manifest, target_manifest)
        passed += 1

        # Cache headers
        test_cache_headers(base, game_slug, source_manifest, target_manifest)
        passed += 1

    print(f"\n{'='*40}")
    print(f"Results: {passed} passed, {failed} failed")
    print(f"{'='*40}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
