# toyssd

[![build-and-test](https://github.com/cphurley82/toyssd/actions/workflows/build.yml/badge.svg)](https://github.com/cphurley82/toyssd/actions/workflows/build.yml)
[![Formatting (clang-format)](https://github.com/cphurley82/toyssd/actions/workflows/format.yml/badge.svg)](https://github.com/cphurley82/toyssd/actions/workflows/format.yml)

A modular, **SystemC/TLM**-based SSD simulator scaffold that integrates with **fio**, uses **GoogleTest** for TDD, and ships with **Docker** + **GitHub Actions CI**.  
Authored by **Chris Hurley**. Licensed under **MIT**.

Note on C++ standard: The project defaults to C++20 (no GNU extensions). You can opt into a newer standard by configuring with `-DCMAKE_CXX_STANDARD=23` (or higher, if supported by your toolchain).

## Quick Start (Docker)

This project uses a single-stage Ubuntu 24.04 image for local dev and CI. The image only sets up tools (compilers, cmake, fio, analyzers). Mount your repo at `/src` and run builds/tests inside the container. To avoid root-owned files on your host, run the container as your host user.

Build the image:

```bash
docker build -t toyssd -f Dockerfile .
```

Step 1 — Configure + Build (run as your host user):

```bash
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd cmake --build build -j
```

Step 2 — Run unit tests:

```bash
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd ctest --test-dir build --output-on-failure --no-tests=error
```

Notes:

- The image includes build tools and system fio; it doesn’t build during `docker build`. Build happens when you run commands in the container with your repo mounted.
- The `-v "$PWD":/src -w /src` bind-mount makes your repository available to the container at `/src`.
- The image preinstalls the `uv` CLI and a dedicated Python virtualenv with dev tooling at `/opt/toyssd/.venv` (on PATH by default). Inside the container, you can run tools directly, e.g. `invoke check` or `invoke verify`. If you prefer `uv`, it will reuse `/opt/toyssd/.venv` instead of creating `/src/.venv`.
  - Python dependencies in the image are installed via `pyproject.toml` at build time using `uv sync --extra dev`. If you change Python tooling dependencies, rebuild the image to update the baked environment.
  - By default, we do not commit `uv.lock`; CI publishes a per-run dependency snapshot artifact (`uv.lock`, `uv-requirements.txt`, and `tool-versions.txt`) so you can inspect or reproduce the exact tool versions used in a run.

### Python tooling inside the container

The container already has a ready-to-use venv at `/opt/toyssd/.venv` and sets `UV_PROJECT_ENVIRONMENT` so `uv run` reuses that environment.

Examples:

```bash
# Run all checks + C++ build + tests (uses pre-baked Python env)
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd \
  uv run invoke verify

# Static checks only
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd \
  uv run invoke check

# Lint/format Python
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd \
  uv run invoke py_lint
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd \
  uv run invoke py_format
```

Note: Because the Python tooling is pre-baked into the image, these commands do not install or write packages into your bind-mounted repo. If you add or upgrade Python tooling in `pyproject.toml`, rebuild the image.

---

## Native Build (macOS)

Prerequisites:

- Xcode Command Line Tools (or Xcode): `xcode-select --install`
- CMake 3.28+ and Git (e.g., via Homebrew: `brew install cmake git`)
- Optional: fio (for the demo): `brew install fio`
  - Not required: if not installed, the build will use a bundled fio (built from sources) by default.

### Install prerequisites via Homebrew

```bash
# Ensure Homebrew is up to date
brew update

# Required
brew install cmake git clang-format

# Optional but recommended for dev workflow
brew install ninja ccache clang-tidy cppcheck

# Optional: fio for manual demo runs (build uses bundled fio if not found)
brew install fio

# Optional: Python for tools (if your system Python is missing modules)
brew install python

# Verify clang-format is on PATH and recognized by CMake
clang-format --version

```

Enable clang-tidy on macOS (Homebrew):

```bash
# Install LLVM (includes clang-tidy)
brew install llvm

# Add LLVM's bin dir to your PATH (persist for zsh)
echo 'export PATH="$(brew --prefix llvm)/bin:$PATH"' >> ~/.zshrc
exec zsh

# Verify clang-tidy is visible
clang-tidy --version
```

Notes:

- If `clang-format` is installed, the build will auto-format sources before compiling. If it isn’t, formatting is skipped with a message and the build proceeds.
- You can switch to Ninja by configuring with `-G Ninja`; otherwise, Unix Makefiles are fine on macOS.

Tip: You can also point CMake directly to clang-tidy at configure time if you prefer not to edit PATH:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCLANG_TIDY_EXE="$(brew --prefix llvm)/bin/clang-tidy"
```

### Build and Test

Release:

```bash
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
ctest
```

Debug:

```bash
mkdir -p build-debug && cd build-debug && cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j
ctest
```

Run the fio demo manually (requires fio installed, or use the bundled fio via CTest only):

```bash
# From the repo root, choose the snippet for your OS.

# macOS
ENGINE=$(pwd)/build/libssdsim_engine.so
export SSD_SIM_LIB_PATH="$(pwd)/build/libssdsim.dylib"
export DYLD_LIBRARY_PATH="$(pwd)/build:$(pwd)/build/_deps/systemc-build/src"
fio \
  --ioengine=external:"${ENGINE}" \
  --filename=$(pwd)/config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5

# Linux
ENGINE=$(pwd)/build/libssdsim_engine.so
export SSD_SIM_LIB_PATH="$(pwd)/build/libssdsim.so"
export LD_LIBRARY_PATH="$(pwd)/build:$(pwd)/build/_deps/systemc-build/src"
export LD_PRELOAD="$(pwd)/build/libscmain_stub.so"
fio \
  --ioengine=external:"${ENGINE}" \
  --filename=$(pwd)/config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5
```

Note: The bundled fio is used by the CTest demo. For manual runs, use your system fio or a specific path.

### Customize the demo

You can tweak the CMake demo target via cache variables at configure time (or by reconfiguring your build directory):

- DEMO_CONFIG: Path to the simulator JSON config (default: `${CMAKE_SOURCE_DIR}/config/default.json`)
- DEMO_RW: fio workload type (e.g., `randwrite`, `randread`, `write`, `read`)
- DEMO_SIZE: total size (e.g., `64M`)
- DEMO_BS: block size (e.g., `4k`)
- DEMO_IODEPTH: iodepth (e.g., `8`)
- DEMO_NUMJOBS: number of jobs (e.g., `1`)
- DEMO_RUNTIME_S: runtime in seconds (e.g., `5`)
- FIO_EXE_OVERRIDE: full path to a specific fio binary (otherwise the build will try a system fio or bundled source-built one)
- BUILD_BUNDLED_FIO: ON by default; builds and uses the bundled fio if no system fio is found.

Notes:

- For manual runs, set SSD_SIM_LIB_PATH and the appropriate dynamic loader variables as shown in the snippets above.

For the automated CTest demo, you can tweak defaults at configure time:

```bash
# From your build dir
cmake -DDEMO_RW=randread -DDEMO_BS=16k -DDEMO_IODEPTH=16 -DDEMO_RUNTIME_S=2 -S .. -B .
```

### Demo entry point

- FioDemo (CTest, labeled "demo")
  - Purpose: A fast, reproducible demo run via CTest (automation-ready).
  - How: `(cd <build_dir> && ctest -L demo)` or simply run from your build directory:
    
    ```bash
    ctest -L demo --output-on-failure
    ```
    
  - Notes: Uses DEMO_* variables and optional `FIO_EXE_OVERRIDE` at configure time.

## Native Build (Linux)

Prerequisites (Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y build-essential cmake git clang-format \
    ninja-build ccache clang-tidy cppcheck fio python3 python3-pip
```

Notes:

- `fio` is optional for manual runs; the project can build and use a bundled fio for the CTest demo when a system fio isn’t present.
- You can switch to Ninja by configuring with `-G Ninja`.

### Build and run tests

Release:

```bash
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
ctest
```

Debug:

```bash
mkdir -p build-debug && cd build-debug && cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j
ctest
```

Run the fio demo manually:

```bash
# From the repo root
ENGINE=$(pwd)/build/libssdsim_engine.so
export SSD_SIM_LIB_PATH="$(pwd)/build/libssdsim.so"
export LD_LIBRARY_PATH="$(pwd)/build:$(pwd)/build/_deps/systemc-build/src"
export LD_PRELOAD="$(pwd)/build/libscmain_stub.so"
fio \
  --ioengine=external:"${ENGINE}" \
  --filename=$(pwd)/config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5
```

Note: For automation or if `fio` is not installed, prefer the CTest demo: `ctest -L demo` from your build directory.

## Repo Structure

- `api/` — C API exposed to fio ioengine
- `sim/` — SystemC simulator modules (host, firmware with FTL, NAND)
- `fio_plugin/` — fio ioengine code and demo job
- `tests/` — GoogleTest unit tests
- `tools/` — Python scripts for running/analyzing
- `docs/` — design document
- `config/` — sample NAND/controller config
- `.github/workflows/` — CI pipeline

See `docs/toy_ssd_simulator_design.md` for full design details.

Tip (VS Code): You can use the built-in tasks to configure and build without typing commands:

- Task: "Configure (Debug)" → runs `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug`
- Task: "Build (Debug)" → runs `cmake --build build-debug -j`

## Code formatting (clang-format)

This repo uses clang-format with the Google C++ Style Guide and C++20.

- Config: `.clang-format` at the repo root (BasedOnStyle: Google, ColumnLimit: 80, Cpp20)
- Editor: VS Code formats on save for C/C++ (`.vscode/settings.json`)
- CI: Formatting workflow runs on PRs and pushes to `main` and fails on style violations (badge above)
- CMake: Building core targets auto-runs formatting first (targets depend on `format`)

### Run formatting locally

Option A — via CMake targets (recommended):

```bash
# Configure once (Debug or Release)
cmake -S . -B build

# Format all tracked C/C++ sources in-place
cmake --build build --target format

# Verify formatting without changing files (fails on diffs)
cmake --build build --target format-check
```

Notes:

- If `clang-format` isn’t installed, CMake’s `format`/`format-check` targets become no-ops and print a hint.
- On macOS, install via Homebrew: `brew install clang-format`.
- The build also depends on `format`, so running `cmake --build` will format sources automatically when `clang-format` is available.

---

License: MIT (see `LICENSE`).

## Troubleshooting

- Dynamic library not found / unresolved symbols when running the manual fio demo:
  - macOS: ensure `DYLD_LIBRARY_PATH` includes your `build` and `build/_deps/systemc-build/src` directories, and `SSD_SIM_LIB_PATH` points to `libssdsim.dylib`.
  - Linux: ensure `LD_LIBRARY_PATH` includes your `build` and `build/_deps/systemc-build/src`, and `SSD_SIM_LIB_PATH` points to `libssdsim.so`; also set `LD_PRELOAD` to `libscmain_stub.so` as shown above.
- Why is the fio ioengine `.so` even on macOS? The fio external ioengine convention uses `.so`, and this project follows that for compatibility.
- `clang-format` missing: CMake's `format`/`format-check` targets will no-op with a hint; builds still proceed.
- `clang-tidy` not found:
  - macOS: `brew install llvm` then add `$(brew --prefix llvm)/bin` to your PATH (see steps above), or pass `-DCLANG_TIDY_EXE=$(brew --prefix llvm)/bin/clang-tidy` at configure time.
  - Ubuntu/Debian: `sudo apt install clang-tidy`.
  - After updating PATH, re-open your shell (or `exec zsh`) and reconfigure your build directory.
- Prefer bundled vs system fio: For reproducible CI-like runs, rely on the CTest demo (bundled fio). For manual experimentation, install fio or provide a path via `-DFIO_EXE_OVERRIDE=/path/to/fio` at configure time.

## Python tooling (uv + Invoke)

This repo uses [uv](https://github.com/astral-sh/uv) for fast Python env management and [Invoke](https://www.pyinvoke.org/) for tasks. Dev dependencies include cpplint, ruff, mypy, pytest, and gcovr.

### One-time setup

Native host environment:

```bash
# Install dev extras into .venv/ (on your host)
uv sync --all-extras
```

Docker environment:

- No setup required. The container image includes a ready-to-use virtualenv at `/opt/toyssd/.venv` with all Python dev tooling installed. Use `invoke …` directly.
  - Default policy: we do not commit `uv.lock`. CI publishes snapshot artifacts (`uv.lock`, an exported `uv-requirements.txt`, and a `tool-versions.txt` manifest) per run for traceability.

### Common tasks

```bash
# Full validation: static checks + C++ build + CTest
uv run invoke verify

# Static checks only (no build/tests)
uv run invoke check

# Format and lint Python
uv run invoke py_format
uv run invoke py_lint
uv run invoke py_typecheck

# C++ configure/build/test (native)
uv run invoke cpp_configure
uv run invoke cpp_build
uv run invoke cpp_test
```

Docker backend is also available by setting `TOYSSD_BACKEND=docker` or using the `docker_` aliases (e.g., `invoke docker_cpp_build`) from inside the container, or `uv run invoke docker_cpp_build` from your host.

Tooling PATH notes (macOS + VS Code)

- clang-tidy is part of Homebrew’s llvm keg at `$(brew --prefix llvm)/bin`. Ensure that directory is on PATH before running `env-check`, CMake, or CTest.
- If you use VS Code CMake Tools, it does not automatically inherit your login shell PATH. You can add PATH (and Python venv variables) to `cmake.configureEnvironment` in `.vscode/settings.json`:

```json
{
  "cmake.configureEnvironment": {
    "PATH": "${env:PATH}:$(/opt/homebrew/bin/brew --prefix llvm)/bin",
    "VIRTUAL_ENV": "${workspaceFolder}/.venv",
    "Python3_EXECUTABLE": "${workspaceFolder}/.venv/bin/python"
  }
}
```

- Alternatively, pass `-DCLANG_TIDY_EXE=$(brew --prefix llvm)/bin/clang-tidy` when configuring to bypass PATH.

### CLI helper

A small CLI is provided under the `toyssd` package (installed in editable mode for development). Install the project and use the CLI:

```bash
# Install the local package in editable mode
uv pip install -e .

# Generate a config
uv run toyssd gen-config --out config/generated.json

# Run the bundled fio demo via CTest (requires a configured build dir)
uv run toyssd run-fio-demo --build-dir build-debug
```

Optional plotting/analysis commands require the `viz` extras (pandas/matplotlib). Install with:

```bash
uv sync --extra viz
```

## Code coverage (local + CI)

This repo provides an opt-in coverage build that works locally and in CI using gcovr. When enabled, tests run with instrumentation and a `coverage` target generates HTML and XML reports and enforces a minimum threshold.

### Install tools

- Python + gcovr
  - Recommended: `uv sync --all-extras` (installs gcovr in `.venv/`)
  - or Homebrew: `brew install gcovr`
- macOS (AppleClang): install LLVM tools for best results: `brew install llvm`
  - gcovr will auto-detect `llvm-cov` when present.

### Run locally

```bash
# Configure with coverage enabled (Debug recommended) and set a threshold
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_CODE_COVERAGE=ON -DCODE_COVERAGE_THRESHOLD=60

# Build tests and libs
cmake --build build-coverage -j

# Run tests, generate HTML report, and enforce the threshold
cmake --build build-coverage --target coverage

# Open the HTML report
open build-coverage/coverage/index.html  # macOS
# xdg-open build-coverage/coverage/index.html  # Linux
```

Notes:

- Threshold is enforced via gcovr's `--fail-under-lines`; adjust with `-DCODE_COVERAGE_THRESHOLD=<N>`.
- External deps and test sources are excluded by default. Customize in `CMakeLists.txt` if needed.
- On macOS with AppleClang, the build uses GCC-style coverage flags and, when available, `llvm-cov gcov` to read coverage data.

### CI

GitHub Actions includes a `coverage` job that builds with instrumentation, runs gcovr, enforces a threshold (60% by default), and uploads the HTML report as an artifact.

