#!/usr/bin/env python3
"""Generate manifests and indexes for all versions already in the store.

Unlike `downgrade-tool ingest`, this does NOT copy files — it works on
files already in place. Use this after downloading versions directly
into the store layout.

Usage:
    python scripts/generate-manifests.py \
        --store-root ./store \
        --games-config config/games.json \
        --game skyrim-se
"""
import argparse
import json
import sys
from pathlib import Path

# Add the common and cli packages to the path
sys.path.insert(0, str(Path(__file__).parent.parent / "common" / "src"))
sys.path.insert(0, str(Path(__file__).parent.parent / "cli" / "src"))

from downgrade_common.config import load_games, find_game
from downgrade_patcher.manifest import generate_manifest, build_manifest_index, build_hash_index
from downgrade_patcher.store import VersionStore


def main():
    parser = argparse.ArgumentParser(description="Generate manifests for existing store versions")
    parser.add_argument("--store-root", required=True, type=Path)
    parser.add_argument("--games-config", required=True, type=Path)
    parser.add_argument("--game", required=True, help="Game slug")
    args = parser.parse_args()

    store = VersionStore(args.store_root)
    games = load_games(args.games_config)
    game_config = find_game(games, args.game)
    if game_config is None:
        print(f"Unknown game: {args.game}")
        sys.exit(1)

    versions = store.list_versions(args.game)
    if not versions:
        print(f"No versions found for {args.game} in {args.store_root}")
        sys.exit(1)

    print(f"Found {len(versions)} versions: {', '.join(versions)}")

    # Generate manifest for each version
    all_manifests = {}
    for version in versions:
        manifest_path = store.manifest_path(args.game, version)
        version_dir = store.version_dir(args.game, version)

        if manifest_path.exists():
            print(f"  {version}: manifest already exists, loading")
            all_manifests[version] = json.loads(manifest_path.read_text())
            continue

        print(f"  {version}: generating manifest...")
        manifest = generate_manifest(args.game, version, version_dir)
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(json.dumps(manifest, indent=2))
        all_manifests[version] = manifest
        print(f"  {version}: {len(manifest['files'])} files")

    # Build manifest index
    print("\nBuilding manifest index...")
    index = build_manifest_index(args.game, game_config, all_manifests)
    index_path = store.manifest_index_path(args.game)
    index_path.write_text(json.dumps(index, indent=2))
    print(f"  {len(index['versions'])} versions indexed")

    # Build hash index
    print("Building hash index...")
    hash_index = build_hash_index(args.game, all_manifests, args.store_root)
    hash_index_path = store.hash_index_path(args.game)
    hash_index_path.write_text(json.dumps(hash_index, indent=2))
    print(f"  {len(hash_index)} unique file hashes")

    print("\nDone!")


if __name__ == "__main__":
    main()
