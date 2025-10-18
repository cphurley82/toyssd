FROM ubuntu:24.04

LABEL org.opencontainers.image.title="toyssd" \
      org.opencontainers.image.description="SystemC/TLM-based SSD simulator scaffold integrating with fio and GoogleTest" \
      org.opencontainers.image.source="https://github.com/cphurley82/toyssd" \
      org.opencontainers.image.licenses="MIT"

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config \
    python3 python3-venv python3-pip \
    clang-format clang-tidy cppcheck \
    libaio-dev zlib1g-dev libnuma-dev fio \
    ca-certificates curl wget \
 && (apt-get install -y --no-install-recommends libaio1 \
     || apt-get install -y --no-install-recommends libaio1t64) \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# Optional Python venv for tools
RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --upgrade pip \
 && /opt/venv/bin/pip install --no-cache-dir matplotlib pandas

FROM ubuntu:24.04

LABEL org.opencontainers.image.title="toyssd" \
    org.opencontainers.image.description="SystemC/TLM-based SSD simulator scaffold integrating with fio and GoogleTest" \
    org.opencontainers.image.source="https://github.com/cphurley82/toyssd" \
    org.opencontainers.image.licenses="MIT"

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential cmake git pkg-config \
  python3 python3-pip \
  clang-format clang-tidy cppcheck \
  libaio-dev zlib1g-dev libnuma-dev fio \
  ca-certificates curl wget \
 && (apt-get install -y --no-install-recommends libaio1 \
   || apt-get install -y --no-install-recommends libaio1t64) \
 && rm -rf /var/lib/apt/lists/*

# Default working directory for bind-mounted source
WORKDIR /src

# Keep it simple: no source copy or build during image creation
# Use this image as a build/runtime environment by mounting your repo:
#   docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "cmake -S . -B build && cmake --build build -j"

ENTRYPOINT ["/bin/bash"]
# Small helpers to run demos quickly
