#!/bin/bash
set -euo pipefail

# Download all known Skyrim SE versions and ingest into the version store
# Usage: ./scripts/download-all-skyrim.sh

STEAM_USER="${STEAM_USER:?Set STEAM_USER environment variable}"
STORE_ROOT="${STORE_ROOT:-./store}"
STAGING_DIR="${STORE_ROOT}/_staging"
GAMES_CONFIG="$(dirname "$0")/../config/games.json"
APP_ID=489830

# Depot IDs
DEPOT_DATA=489831    # BSAs, ESMs (bulk data)
DEPOT_CORE=489832    # Core game files
DEPOT_EXE=489833     # SkyrimSE.exe

# Version -> manifest IDs (data, core, exe)
declare -A VERSIONS
VERSIONS["1.5.97"]="7848722008564294070 8702665189575304780 2289561010626853674"
VERSIONS["1.6.317"]="8893533186410323048 7231497621745539234 5887811902658527321"
VERSIONS["1.6.323"]="6913241376784585188 6077137133470860357 2414608533287116506"
VERSIONS["1.6.353"]="1476684358338706955 7089166303853251347 4570833277049890269"
VERSIONS["1.6.640"]="3660787314279169352 2756691988703496654 5291801952219815735"
VERSIONS["1.6.1130"]="3737743381894105176 4341968404481569190 2442187225363891157"
VERSIONS["1.6.1170"]="8442952117333549665 8042843504692938467 1914580699073641964"

mkdir -p "$STAGING_DIR"

for VERSION in $(echo "${!VERSIONS[@]}" | tr ' ' '\n' | sort -V); do
    read -r MANIFEST_DATA MANIFEST_CORE MANIFEST_EXE <<< "${VERSIONS[$VERSION]}"

    DEST="$STORE_ROOT/skyrim-se/$VERSION"
    if [ -d "$DEST" ]; then
        echo "=== Skipping $VERSION (already exists at $DEST) ==="
        continue
    fi

    STAGE="$STAGING_DIR/skyrim-se-$VERSION"
    mkdir -p "$STAGE"

    echo ""
    echo "============================================"
    echo "=== Downloading Skyrim SE $VERSION ==="
    echo "============================================"
    echo ""

    echo "--- Depot $DEPOT_DATA (data) manifest $MANIFEST_DATA ---"
    DepotDownloader -app $APP_ID -depot $DEPOT_DATA -manifest "$MANIFEST_DATA" \
        -username "$STEAM_USER" -remember-password -dir "$STAGE" || {
        echo "ERROR: Failed to download depot $DEPOT_DATA for $VERSION"
        continue
    }

    echo "--- Depot $DEPOT_CORE (core) manifest $MANIFEST_CORE ---"
    DepotDownloader -app $APP_ID -depot $DEPOT_CORE -manifest "$MANIFEST_CORE" \
        -username "$STEAM_USER" -remember-password -dir "$STAGE" || {
        echo "ERROR: Failed to download depot $DEPOT_CORE for $VERSION"
        continue
    }

    echo "--- Depot $DEPOT_EXE (exe) manifest $MANIFEST_EXE ---"
    DepotDownloader -app $APP_ID -depot $DEPOT_EXE -manifest "$MANIFEST_EXE" \
        -username "$STEAM_USER" -remember-password -dir "$STAGE" || {
        echo "ERROR: Failed to download depot $DEPOT_EXE for $VERSION"
        continue
    }

    echo ""
    echo "--- Ingesting $VERSION into store ---"

    # Move staged files into store layout directly
    mkdir -p "$(dirname "$DEST")"
    mv "$STAGE" "$DEST"

    echo "=== $VERSION complete at $DEST ==="
done

# Clean up staging dir if empty
rmdir "$STAGING_DIR" 2>/dev/null || true

echo ""
echo "============================================"
echo "=== All downloads complete ==="
echo "============================================"
echo ""
echo "Store root: $STORE_ROOT"
echo ""
echo "Run manifest generation separately (does not copy files):"
echo "  cd $(dirname "$0")/.."
echo "  python scripts/generate-manifests.py --store-root $STORE_ROOT --games-config config/games.json --game skyrim-se"
