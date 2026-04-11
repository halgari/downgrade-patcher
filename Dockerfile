FROM python:3.12-slim

# No zstd CLI needed — using pyzstd library directly

WORKDIR /app

# Install common library first (changes less often)
COPY common/pyproject.toml common/pyproject.toml
COPY common/src/ common/src/
RUN pip install --no-cache-dir ./common

# Install server
COPY server/pyproject.toml server/pyproject.toml
COPY server/src/ server/src/
RUN pip install --no-cache-dir ./server

# Copy game config
COPY config/ /app/config/

# Default paths — override with environment variables
ENV DOWNGRADE_STORE_ROOT=/data/store
ENV DOWNGRADE_CACHE_ROOT=/data/cache
ENV DOWNGRADE_CONFIG_DIR=/app/config

EXPOSE 8000

CMD ["uvicorn", "patch_server.__main__:app", "--host", "0.0.0.0", "--port", "8000"]
