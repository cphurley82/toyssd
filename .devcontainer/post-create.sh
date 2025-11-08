#!/usr/bin/env bash
#
# post-create.sh — finalizes the VS Code devcontainer after it is built/launched.
#
# Goals:
#   1. Ensure the `uv` tool is present even when the devcontainer image was rebuilt locally.
#   2. Validate that SystemC has been installed under ${SYSTEMC_PREFIX}; install it if missing.
#   3. Run `invoke bootstrap` inside the mounted workspace so CMake configs and Python deps
#      are ready for the VS Code session.
#
# This script is intentionally idempotent: it skips work when uv/SystemC already exist,
# which is the normal case for published Docker images or Codespaces environments.

set -euo pipefail

if ! command -v uv >/dev/null 2>&1; then
    curl -LsSf https://astral.sh/uv/install.sh | sudo sh -s -- --install-dir /usr/local/bin
fi

SYSTEMC_PREFIX="${TOYSSD_SYSTEMC_PREFIX:-/opt/systemc}"
if [ ! -d "${SYSTEMC_PREFIX}/lib" ]; then
    # Run the installer with sudo so /opt/systemc is writable even for dev users.
    sudo -E SYSTEMC_PREFIX="${SYSTEMC_PREFIX}" \
        .devcontainer/install_systemc.sh
fi

# Re-run the Invoke bootstrap so CMake build trees + Python deps exist even if
# the workspace was freshly cloned or shared via VS Code Codespaces.
invoke bootstrap
