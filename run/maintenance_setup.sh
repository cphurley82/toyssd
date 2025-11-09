#!/usr/bin/env bash
# Maintenance script for cached Codex Universal containers.
# - Reapplies environment exports and re-syncs uv dependencies without reinstalling toolchains.
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export TOYSSD_SYSTEMC_PREFIX=/opt/systemc
export TOYSSD_FETCH_SYSTEMC=OFF
export CC=clang
export CXX=clang++
export PATH="$HOME/.local/bin:$PATH"
export UV_SYSTEM_PREFER=1

# Cached containers might still have uv but reinstall it if it was removed.
if ! command -v uv >/dev/null 2>&1; then
  curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh
fi

# Surface a clear error if the initial setup never ran (missing SystemC install).
if [[ ! -f "${TOYSSD_SYSTEMC_PREFIX}/lib/libsystemc.so" ]]; then
  echo "SystemC not found under ${TOYSSD_SYSTEMC_PREFIX}; run ./run/setup.sh" >&2
  exit 1
fi

cd "${REPO_ROOT}"
uv sync --extra dev

echo "toyssd maintenance sync complete."
