#!/usr/bin/env bash
# Setup script for Codex Universal containers.
# - Mirrors the dev Dockerfile toolchain and installs SystemC/uv without running CMake configure/build.
# - Designed to run once in a fresh container; follow up sessions should use maintenance_setup.sh.
set -euo pipefail

if [[ -t 1 ]]; then
  set -x
fi

REPO_ROOT="$(cd -- "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSTEMC_VERSION="3.0.2"
SYSTEMC_SHA256="9b3693ed286aab958b9e5d79bb0ad3bc523bbc46931100553275352038f4a0c4"
SYSTEMC_PREFIX="/opt/systemc"
# Number of parallel build jobs for the SystemC build (tunable via env to avoid OOM).
SYSTEMC_BUILD_JOBS="${SYSTEMC_BUILD_JOBS:-2}"
# Apt packages are based on Dockerfile:25-44, with additional Python packages for bare systems.
APT_PACKAGES=(
  build-essential
  clang
  clang-format
  clang-tidy
  ccache
  cmake
  curl
  git
  libedit-dev
  libffi-dev
  libssl-dev
  ninja-build
  pkg-config
  sudo
  tar
  unzip
  xz-utils
  python3
  python3-dev
  python3-pip
  python3-venv
)

if command -v sudo >/dev/null 2>&1; then
  SUDO="sudo"
else
  SUDO=""
fi

if [[ -z "${SUDO}" && "${EUID}" -ne 0 ]]; then
  echo "This script requires root privileges (sudo not available)." >&2
  exit 1
fi

# Install only the missing packages to keep the script idempotent.
missing=()
for pkg in "${APT_PACKAGES[@]}"; do
  if ! dpkg -s "${pkg}" >/dev/null 2>&1; then
    missing+=("${pkg}")
  fi
done

if [[ ${#missing[@]} -gt 0 ]]; then
  ${SUDO} apt-get update
  ${SUDO} apt-get install -y --no-install-recommends "${missing[@]}"
fi

# Install uv via the official installer only when unavailable on PATH.
if ! command -v uv >/dev/null 2>&1; then
  curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh
fi
export UV_SYSTEM_PREFER=1

# Build+install SystemC into /opt/systemc so invoke bootstrap can reuse it.
install_systemc() {
  local workdir
  workdir="$(mktemp -d)"
  trap 'rm -rf "'"${workdir}"'"' EXIT
  curl -L "https://github.com/accellera-official/systemc/archive/refs/tags/${SYSTEMC_VERSION}.tar.gz" \
    -o "${workdir}/systemc.tar.gz"
  echo "${SYSTEMC_SHA256}  ${workdir}/systemc.tar.gz" | sha256sum -c -
  tar -xf "${workdir}/systemc.tar.gz" -C "${workdir}" --strip-components=1
  cmake -S "${workdir}" -B "${workdir}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
    -DCMAKE_INSTALL_PREFIX="${SYSTEMC_PREFIX}"
  cmake --build "${workdir}/build" --parallel "${SYSTEMC_BUILD_JOBS}"
  ${SUDO} cmake --install "${workdir}/build"
  rm -rf "${workdir}"
  trap - EXIT
}

if [[ ! -f "${SYSTEMC_PREFIX}/lib/libsystemc.so" ]]; then
  install_systemc
fi

# Persist environment exports so future shells inherit compiler hints/SystemC location.
profile_snippet=$'# toyssd environment\nexport TOYSSD_SYSTEMC_PREFIX=/opt/systemc\nexport TOYSSD_FETCH_SYSTEMC=OFF\nexport UV_SYSTEM_PREFER=1\nexport CC=clang\nexport CXX=clang++\nexport PATH="$HOME/.local/bin:$PATH"\n'
if [[ -d /etc/profile.d && -w /etc/profile.d ]]; then
  printf '%s' "${profile_snippet}" | ${SUDO} tee /etc/profile.d/toyssd.sh >/dev/null
else
  printf '\n%s' "${profile_snippet}" >> "${HOME}/.profile"
fi

cd "${REPO_ROOT}"
# Sync Python deps without configuring CMake (invoke bootstrap handles that later).
uv sync --extra dev

echo "toyssd setup complete. Start a new shell (to pick up env vars) before running 'uv run invoke bootstrap'."
