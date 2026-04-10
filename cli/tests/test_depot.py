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
