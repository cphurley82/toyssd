#!/usr/bin/env bash
#
# install_systemc.sh — idempotently build and install SystemC inside devcontainers.
#
# Why this script exists:
#   - VS Code's devcontainer lifecycle can rebuild images locally or in Codespaces.
#   - When that happens we want to verify SystemC is present under ${SYSTEMC_PREFIX}
#     (default /opt/systemc) without duplicating cmake logic across multiple files.
#   - If the directory already exists we exit quickly; otherwise we download,
#     configure, build, and install the pinned SystemC release.
#
# Usage:
#   SYSTEMC_PREFIX=/opt/systemc .devcontainer/install_systemc.sh
#   (Prefix defaults to /opt/systemc when unspecified.)
#
# Requirements:
#   - curl, cmake, build-essential must already be installed (the Dockerfile ensures this).
#   - Script must run with permissions to write to ${SYSTEMC_PREFIX}.

set -euo pipefail

SYSTEMC_VERSION="3.0.2"
SYSTEMC_URL="https://github.com/accellera-official/systemc/archive/refs/tags/${SYSTEMC_VERSION}.tar.gz"
SYSTEMC_SHA256="9b3693ed286aab958b9e5d79bb0ad3bc523bbc46931100553275352038f4a0c4"
SYSTEMC_PREFIX="${SYSTEMC_PREFIX:-/opt/systemc}"

if [ -d "${SYSTEMC_PREFIX}/lib" ]; then
    echo "SystemC already present under ${SYSTEMC_PREFIX}; skipping install."
    exit 0
fi

tmpdir="$(mktemp -d)"
cleanup() {
    rm -rf "${tmpdir}"
}
trap cleanup EXIT

tarball="${tmpdir}/systemc.tar.gz"
curl -L "${SYSTEMC_URL}" -o "${tarball}"
echo "${SYSTEMC_SHA256}  ${tarball}" | sha256sum -c -
tar -xzf "${tarball}" -C "${tmpdir}"
src_dir="${tmpdir}/systemc-${SYSTEMC_VERSION}"
# Configure a Release build that installs directly into ${SYSTEMC_PREFIX}.
cmake -S "${src_dir}" -B "${src_dir}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${SYSTEMC_PREFIX}"
# Build + install in one shot; --parallel uses all cores available in container.
cmake --build "${src_dir}/build" --target install --parallel

echo "SystemC ${SYSTEMC_VERSION} installed to ${SYSTEMC_PREFIX}"
