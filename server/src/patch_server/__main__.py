"""Entry point for running the server: python -m patch_server"""
import uvicorn
from patch_server.settings import Settings
from patch_server.app import create_app

settings = Settings()
app = create_app(
    store_root=settings.store_root,
    cache_root=settings.cache_root,
    config_dir=settings.config_dir,
)

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
