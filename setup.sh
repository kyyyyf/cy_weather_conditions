#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# ── require apt ───────────────────────────────────────────────────────────────
if ! command -v apt-get &>/dev/null; then
    echo "ERROR: apt-get not found — Ubuntu/Debian required." >&2
    exit 1
fi

# ── install missing packages ──────────────────────────────────────────────────
NEEDED=()
for pkg in cmake ninja-build gcc-14 g++-14 libsqlite3-dev libcurl4-openssl-dev git ca-certificates; do
    dpkg -s "$pkg" &>/dev/null 2>&1 || NEEDED+=("$pkg")
done

if [ "${#NEEDED[@]}" -gt 0 ]; then
    echo "==> Installing: ${NEEDED[*]}"
    sudo apt-get update -qq
    sudo apt-get install -y "${NEEDED[@]}"
else
    echo "==> All dependencies already installed."
fi

# ── build ─────────────────────────────────────────────────────────────────────
echo "==> Building (gcc-14, $(nproc) jobs)..."
make CC=gcc-14 CXX=g++-14 -j"$(nproc)"

# ── runtime directories ───────────────────────────────────────────────────────
mkdir -p db

echo ""
echo "==> Build complete."
echo "    collector : weather-collector-cpp/build/weather-collector"
echo "    dashboard : weather-dashboard-cpp/build/weather-dashboard"
echo ""
echo "Next steps:"
echo "    Run collector once : cd weather-collector-cpp && ./build/weather-collector config.toml"
echo "    Start dashboard    : ./run-dashboard.sh"
