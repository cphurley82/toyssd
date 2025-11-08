# syntax=docker/dockerfile:1.6

ARG DEBIAN_FRONTEND=noninteractive
ARG SYSTEMC_VERSION=3.0.2
ARG SYSTEMC_URL=https://github.com/accellera-official/systemc/archive/refs/tags/${SYSTEMC_VERSION}.tar.gz
ARG SYSTEMC_SHA256=9b3693ed286aab958b9e5d79bb0ad3bc523bbc46931100553275352038f4a0c4
ARG USERNAME=toyssd
ARG USER_UID=1000
ARG USER_GID=1000

################################################################################
# Base stage — compiler toolchain, Python 3.11, uv, Invoke deps.
################################################################################
FROM python:3.11-bookworm AS base
ARG USERNAME
ARG USER_UID
ARG USER_GID

ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND} \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    UV_SYSTEM_PREFER=1

# Build essentials + clang toolchain + quality-of-life utilities.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        clang-format \
        clang-tidy \
        ccache \
        cmake \
        curl \
        git \
        libedit-dev \
        libffi-dev \
        libssl-dev \
        ninja-build \
        pkg-config \
        sudo \
        tar \
        unzip \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Install uv globally for deterministic Python dependency management.
RUN curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh

# Create an unprivileged user that mirrors the devcontainer defaults.
RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && mkdir -p /etc/sudoers.d \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME}

WORKDIR /tmp/toyssd

# Install project dependencies system-wide (editable sources stay bind-mounted).
COPY pyproject.toml ./
RUN uv export --extra dev --format requirements.txt \
        --no-emit-project --no-emit-workspace --no-emit-local \
        --output-file /tmp/requirements.txt \
    && uv pip install --system --require-hashes --requirements /tmp/requirements.txt \
    && rm -rf /tmp/toyssd /tmp/requirements.txt /root/.cache

################################################################################
# Dev stage — adds SystemC toolchain and defaults for interactive development.
################################################################################
FROM base AS dev
ARG USERNAME
ARG SYSTEMC_URL
ARG SYSTEMC_SHA256

ENV TOYSSD_SYSTEMC_PREFIX=/opt/systemc \
    TOYSSD_FETCH_SYSTEMC=OFF \
    CC=clang \
    CXX=clang++

# Build and install SystemC once per architecture to /opt/systemc. We force a
# C++20 build so the exported API version matches the rest of the project (and
# avoids linker mismatches such as sc_api_version_3_0_2_cxx202002L).
RUN mkdir -p /tmp/systemc \
    && curl -L "${SYSTEMC_URL}" -o /tmp/systemc.tar.gz \
    && echo "${SYSTEMC_SHA256}  /tmp/systemc.tar.gz" | sha256sum -c - \
    && tar -xf /tmp/systemc.tar.gz -C /tmp/systemc --strip-components=1 \
    && cmake -S /tmp/systemc -B /tmp/systemc/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DCMAKE_INSTALL_PREFIX=${TOYSSD_SYSTEMC_PREFIX} \
    && cmake --build /tmp/systemc/build --parallel \
    && cmake --install /tmp/systemc/build \
    && rm -rf /tmp/systemc /tmp/systemc.tar.gz

# Ensure ccache directories exist for bind-mounted volumes.
RUN mkdir -p /home/${USERNAME}/.cache/ccache \
    && chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}/.cache

USER ${USERNAME}
WORKDIR /workspaces/toyssd
ENV PATH="/home/${USERNAME}/.local/bin:${PATH}"
CMD ["/bin/bash"]

################################################################################
# Builder stage — produce Python wheel/install artifacts for runtime image.
################################################################################
FROM dev AS builder
USER root
WORKDIR /src
COPY . /src

# Ensure the PEP 517 build frontend is present.
RUN python -m pip install --no-cache-dir build

RUN CMAKE_PREFIX_PATH=${TOYSSD_SYSTEMC_PREFIX} \
    python -m build --wheel --outdir /tmp/dist

################################################################################
# Runtime stage — slim image for executing workloads.
################################################################################
FROM python:3.11-slim AS runtime

ENV PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    TOYSSD_SYSTEMC_PREFIX=/opt/systemc \
    TOYSSD_FETCH_SYSTEMC=OFF \
    LD_LIBRARY_PATH=/opt/systemc/lib:$LD_LIBRARY_PATH

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libstdc++6 \
        libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=dev /opt/systemc /opt/systemc
COPY --from=builder /tmp/dist /tmp/dist
RUN pip install --no-cache-dir /tmp/dist/*.whl \
    && rm -rf /tmp/dist

WORKDIR /work
CMD ["bash"]
