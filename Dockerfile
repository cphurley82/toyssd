FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
	build-essential cmake git python3 python3-pip python3-venv \
	clang-format clang-tidy cppcheck pkg-config \
	libaio-dev zlib1g-dev fio \
	wget curl ca-certificates vim && \
	rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# Create a virtual environment to avoid PEP 668 restrictions and install Python tools
RUN python3 -m venv /opt/venv \
 && /opt/venv/bin/pip install --upgrade pip \
 && /opt/venv/bin/pip install --no-cache-dir matplotlib pandas

# Make the venv default for any subsequent shell
ENV PATH="/opt/venv/bin:${PATH}"

# Remove any pre-existing host build artifacts that may have been copied in
RUN rm -rf build build-debug build-copilot

# Configure and build the project, then build a matching fio from fetched sources
RUN mkdir -p build \
 && cd build \
 && cmake .. \
 && make -j$(nproc) \
 && make -C _deps/fio-src -j$(nproc)

ENTRYPOINT ["/bin/bash"]
