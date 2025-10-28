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

Notes:

- If `clang-format` is installed, the build will auto-format sources before compiling. If it isn’t, formatting is skipped with a message and the build proceeds.
- You can switch to Ninja by configuring with `-G Ninja`; otherwise, Unix Makefiles are fine on macOS.

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
- Prefer bundled vs system fio: For reproducible CI-like runs, rely on the CTest demo (bundled fio). For manual experimentation, install fio or provide a path via `-DFIO_EXE_OVERRIDE=/path/to/fio` at configure time.

## Python venv + cpplint (C++ style checks)

This repo includes a `cpplint` CTest to check basic Google-style C++ conventions. It's optional and only added when testing is enabled and `cpplint` is installed.

### Create a virtual environment (macOS/Linux)

```bash
# From repo root (macOS zsh / Linux bash)
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r requirements.txt
```

Notes:

- Deactivate later with `deactivate`.
- If your system Python is managed by pyenv/Homebrew, ensure `python3` points to your intended interpreter.

### Configure, build, and run cpplint via CTest

```bash
# Configure (Debug) and build
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j

# Run only the cpplint test
ctest --test-dir build-debug -R cpplint
```

If `cpplint` or `Python3` isn't found, the cpplint CTest is skipped and a hint is printed during CMake configure. The test uses cpplint defaults.
