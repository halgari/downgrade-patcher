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
