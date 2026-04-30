#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

BINARY="weather-dashboard-cpp/build/weather-dashboard"
CONFIG="weather-dashboard-cpp/config.toml"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: binary not found — run ./setup.sh first." >&2
    exit 1
fi

if [ ! -f "$CONFIG" ]; then
    echo "ERROR: $CONFIG not found." >&2
    exit 1
fi

echo "==> Starting weather dashboard (http://0.0.0.0:3000)..."
cd weather-dashboard-cpp
exec ./build/weather-dashboard config.toml
