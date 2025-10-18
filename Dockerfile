##########
# Builder #
##########
FROM ubuntu:24.04 AS builder

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
		ca-certificates curl wget && \
		rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# Optional Python venv for tools (kept in builder only)
RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --upgrade pip \
 && /opt/venv/bin/pip install --no-cache-dir matplotlib pandas

# Clean any host build artifacts and build Release artifacts
RUN rm -rf build build-debug build-copilot \
 && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j

############
# Runtime  #
############
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
	libstdc++6 libgcc-s1 zlib1g libnuma1 fio ca-certificates \
 && (apt-get install -y --no-install-recommends libaio1 \
	 || apt-get install -y --no-install-recommends libaio1t64) \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/toyssd

# Create dirs
RUN mkdir -p /opt/toyssd/lib/systemc /opt/toyssd/bin /opt/toyssd/config /opt/toyssd/tools

# Copy minimal artifacts from builder
# Core libs
COPY --from=builder /workspace/build/libssdsim.so /opt/toyssd/lib/libssdsim.so
COPY --from=builder /workspace/build/libsimlib.so /opt/toyssd/lib/libsimlib.so
# Also pick up libraries placed under build/lib by some CMake generators
COPY --from=builder /workspace/build/lib/*.so /opt/toyssd/lib/
COPY --from=builder /workspace/build/libssdsim_engine.so /opt/toyssd/lib/libssdsim_engine.so
COPY --from=builder /workspace/build/libscmain_stub.so /opt/toyssd/lib/libscmain_stub.so
COPY --from=builder /workspace/build/_deps/systemc-build/src/libsystemc.so* /opt/toyssd/lib/systemc/
COPY --from=builder /workspace/build/tests/unit_tests /opt/toyssd/bin/unit_tests
COPY --from=builder /workspace/config /opt/toyssd/config
COPY --from=builder /workspace/tools /opt/toyssd/tools

# Runtime environment for demos and tests
ENV LD_LIBRARY_PATH=/opt/toyssd/lib:/opt/toyssd/lib/systemc
ENV SSD_SIM_LIB_PATH=/opt/toyssd/lib/libssdsim.so
ENV PATH=/opt/toyssd/bin:${PATH}

# Small helpers to run demos quickly
RUN printf '%s\n' \
	'#!/usr/bin/env bash' \
	'set -euo pipefail' \
	'export LD_PRELOAD=/opt/toyssd/lib/libscmain_stub.so' \
	'FIO_ENGINE="/opt/toyssd/lib/libssdsim_engine.so"' \
	'CONFIG="/opt/toyssd/config/default.json"' \
	'fio --ioengine=external:"${FIO_ENGINE}" --filename="${CONFIG}" --name=demo_short --rw=randwrite --size=4M --bs=4k --iodepth=8 --numjobs=1 --time_based --runtime=1' \
	> /usr/local/bin/toyssd-demo-short && chmod +x /usr/local/bin/toyssd-demo-short && \
	printf '%s\n' \
	'#!/usr/bin/env bash' \
	'set -euo pipefail' \
	'export LD_PRELOAD=/opt/toyssd/lib/libscmain_stub.so' \
	'FIO_ENGINE="/opt/toyssd/lib/libssdsim_engine.so"' \
	'CONFIG="/opt/toyssd/config/default.json"' \
	'fio --ioengine=external:"${FIO_ENGINE}" --filename="${CONFIG}" --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 --time_based --runtime=5' \
	> /usr/local/bin/toyssd-demo && chmod +x /usr/local/bin/toyssd-demo

# Default to an interactive shell; docs show how to run tests or demo
ENTRYPOINT ["/bin/bash"]
