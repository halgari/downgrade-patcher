import json
import subprocess
from pathlib import Path

import click

from downgrade_patcher.config import load_games, find_game
from downgrade_patcher.depot import download_depot
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

        # Split into chunks (patch_file may not exist if subprocess was mocked)
        if patch_file.exists():
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


def _load_all_manifests(store: VersionStore, game_slug: str) -> dict[str, dict]:
    manifests = {}
    manifests_dir = store.manifest_path(game_slug, "dummy").parent
    if not manifests_dir.exists():
        return manifests
    for path in manifests_dir.glob("*.json"):
        data = json.loads(path.read_text())
        manifests[data["version"]] = data
    return manifests
