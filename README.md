# toyssd

`toyssd` is a modular solid-state drive (SSD) simulator that pairs a Python-first API with SystemC TLM‑2.0 models. The project targets rapid prototyping of SSD behaviors while keeping a clear path toward future firmware experimentation.

## Features

- **Hybrid Python/SystemC stack** – Invoke the SystemC kernel directly from Python via PySysC bindings.
- **Composable models** – Host, controller, and NAND building blocks communicate over blocking-transport TLM sockets.
- **Deterministic execution** – Blocking transport only (no temporal decoupling) for reproducible workloads.
- **Rich tooling** – CMake + scikit-build-core, GoogleTest, Invoke automation, uv for dependencies, clang-format/tidy/cpplint.
- **Visualization hook** – ASCII defrag-style renderer driven by events emitted from the NAND model.

## Repository Layout

```text
.
├── CMakeLists.txt           # SystemC/GoogleTest build configuration
├── include/toyssd/          # C++ headers for host/controller/nand/geometry
├── src/                     # C++ model implementations and extensions
├── tests/                   # GoogleTest suites for the SystemC models
├── python/toyssd/           # Python package (ToySSD orchestrator, viz, config)
├── tools/invoke_tasks.py    # Invoke task definitions (build/test/lint/etc.)
├── tasks.py                 # Invoke entry point
├── docs/ssd_sim_design.md   # Full architecture & design document
└── examples/systemc_hello.cpp # Smoke-test example for SystemC toolchain
```

## Getting Started

### Prerequisites

- Python 3.11+ (uv will set up a local virtual environment automatically).
- Xcode Command Line Tools (`xcode-select --install`) so Apple’s SDK-aware Clang and system headers are available.
- CMake ≥ 3.24 and a C++20-capable compiler. Invoke tasks pin `CC=/usr/bin/clang` and `CXX=/usr/bin/clang++` so the Apple Clang toolchain is used by default.
- Ninja (optional but preferred for faster builds; uv/scikit-build-core will fetch it if missing).

### Bootstrap

```bash
uv run invoke bootstrap
```

This command:

1. Installs Python dependencies into a `.venv` (preferring uv-managed environments).
2. Configures a `build/debug` CMake tree that downloads SystemC 3.0.2, GoogleTest, and the pinned PySysC snapshot.

### Build

```bash
uv run invoke build
```

Build outputs land in `build/<config>` (default: `build/debug`). The `toyssd_smoke` custom target builds the SystemC hello example to confirm the toolchain.

### Tests

```bash
uv run invoke test
```

Runs the GoogleTest suites and Python unit tests. The Controller/NAND tests wire up the full path to verify single-page write/read behavior.

### Format & Lint

```bash
uv run invoke format
uv run invoke lint
```

Formatting uses clang-format for C++ and Ruff for Python. Linting runs clang-tidy, cpplint, and Ruff checks.

## Python Usage Example

```python
from toyssd import ToySSD, NandGeometry, SimConfig, Workload

geometry = NandGeometry()
config = SimConfig(nand_geometry=geometry, enable_visualization=True)

sim = ToySSD(config)
workload = Workload.sequential_write(start_lba=0, length_gb=0.001, block_size_kb=4)
sim.run_workload(workload)
print(sim.get_stats().total_writes)
sim.shutdown()
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines, coding standards, and review expectations.
