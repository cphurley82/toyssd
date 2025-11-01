###############################################
# Dockerfile for toyssd (local development)
#
# Purpose:
# - Provide a consistent local development environment with build tools and
#   runtime libraries installed.
# - No source code is copied into the image; you will bind-mount your repo.
# - Not intended for publishing or release.
#
# Quick start:
#   # Build the image
#   docker build -t toyssd .
#
#   # Run commands as your host user to avoid root-owned files:
#   # Configure:
#   docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd cmake -S . -B build
#   # Build:
#   docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd cmake --build build -j
#
#   # Run tests:
#   docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd ctest --test-dir build --output-on-failure
#
#   # Enter an interactive shell inside the container:
#   docker run --rm -it --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd
###############################################

FROM ubuntu:24.04

LABEL org.opencontainers.image.title="toyssd" \
      org.opencontainers.image.description="SystemC/TLM-based SSD simulator scaffold integrating with fio and GoogleTest" \
      org.opencontainers.image.source="https://github.com/cphurley82/toyssd" \
      org.opencontainers.image.licenses="MIT"

ENV DEBIAN_FRONTEND=noninteractive
# Install build tools, analyzers, Python, and runtime libraries needed by the simulator
# Notes:
# - `--no-install-recommends` keeps the image smaller by avoiding optional deps.
# - We clean apt lists at the end to reduce layer size.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config \
    python3 python3-venv \
    clang-format clang-tidy cppcheck \
    libaio-dev zlib1g-dev libnuma-dev fio \
    ca-certificates curl wget \
 && (apt-get install -y --no-install-recommends libaio1 \
     || apt-get install -y --no-install-recommends libaio1t64) \
 && rm -rf /var/lib/apt/lists/*

# Install uv (fast Python package/dependency manager).
# We install the single-file binary and expose it on the global PATH so it is
# available even when running the container as a non-root user.
RUN curl -LsSf https://astral.sh/uv/install.sh | sh \
 && ln -sf /root/.local/bin/uv /usr/local/bin/uv

# Prepare a project metadata directory for dependency resolution via uv.
# We only copy dependency metadata (not source) to keep the image generic.
WORKDIR /opt/toyssd/app
COPY pyproject.toml ./

# Create a dedicated, immutable virtual environment for Python tooling within
# the image using the project's pyproject.toml (avoids /src/.venv conflicts).
# Note: If you add a uv.lock later, copy it here and add `--frozen` for
# reproducible installs.
ENV UV_PROJECT_ENVIRONMENT=/opt/toyssd/.venv
RUN uv sync --extra dev --no-install-project

# Make the prebuilt venv the default Python in the container and hint CMake.
ENV VIRTUAL_ENV=/opt/toyssd/.venv
ENV PATH="/opt/toyssd/.venv/bin:${PATH}" \
    Python3_EXECUTABLE="/opt/toyssd/.venv/bin/python"

# Default working directory for bind-mounted source
WORKDIR /src

# Default command is bash for interactive use; override with a command to run tools directly
CMD ["/bin/bash"]
