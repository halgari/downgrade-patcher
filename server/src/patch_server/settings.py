from pathlib import Path
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    store_root: Path = Path("store")
    cache_root: Path = Path("cache")
    config_dir: Path = Path("config")

    model_config = {"env_prefix": "DOWNGRADE_"}
