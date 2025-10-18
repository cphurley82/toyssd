# toyssd

[![Formatting (clang-format)](https://github.com/cphurley82/toyssd/actions/workflows/format.yml/badge.svg)](https://github.com/cphurley82/toyssd/actions/workflows/format.yml)

A modular, **SystemC/TLM**-based SSD simulator scaffold that integrates with **fio**, uses **GoogleTest** for TDD, and ships with **Docker** + **GitHub Actions CI**.  
Authored by **Chris Hurley**. Licensed under **MIT**.

## Quick Start (Docker)

The repo ships with a minimal Ubuntu 24.04 container that provides the build environment (compilers, cmake, fio, tools). You mount your repo root into it at `/src` and build inside the container.

Build the image:

```bash
docker build -t toyssd -f Dockerfile .
```

Step 1 — Configure + Build (inside container, with your repo mounted at /src):

```bash
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
```

Step 2 — Run unit tests:

```bash
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "(cd build && ctest -R UnitTests --output-on-failure)"
```

Step 3 — Run demos:

```bash
# Short demo target
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "(cd build && cmake --build . --target run_fio_demo_short -j1)"

# Longer demo target
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "(cd build && cmake --build . --target run_fio_demo -j1)"
```

Notes:

- The image includes build tools and system fio; it doesn’t build during docker build. Build occurs when you run commands in the container with your repo mounted.
- The `-v "$PWD":/src -w /src` bind-mount gives the container easy access to everything in your repo at `/src`.
- Targets `run_fio_demo_short` and `run_fio_demo` handle required env vars for the fio engine.

### Validate like CI

Run the same checks CI performs. Two parts: formatting and the build.yml jobs (unit tests + demo).

Formatting check (matches .github/workflows/format.yml):

```bash
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc \
  'set -euo pipefail; \
   echo "CI-format: using clang-format"; \
   clang-format --version; \
   git ls-files | grep -E "\\.(c|h|cpp|hpp)$" | grep -vE "^(build|build-debug|_deps)/" | \
   tee files.txt | xargs -r clang-format --dry-run --Werror; \
   echo FORMAT_CHECK_OK'
```

Build and tests (matches .github/workflows/build.yml):

```bash
# Job: unit-tests — Split into build and test
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "(cd build && ctest -R UnitTests --output-on-failure)"

# Job: demo — Split into build, short CTest demo, and full demo target
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTOYSSD_DEMO_TEST=ON && cmake --build build -j"
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "(cd build && ctest -R FioDemoShort --output-on-failure)"
docker run --rm -t -v "$PWD":/src -w /src toyssd -lc "(cd build && cmake --build . --target run_fio_demo -j1)"
```

---

## Native Build (macOS)

Prerequisites:

- Xcode Command Line Tools (or Xcode): `xcode-select --install`
- CMake 3.16+ and Git (e.g., via Homebrew: `brew install cmake git`)
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

Build (Release):

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
ctest --output-on-failure
```

Build (Debug):

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
(cd build-debug && ctest --output-on-failure)
```

Run only the project tests if you see upstream example tests in the list:

```bash
(cd build || cd build-debug) && ctest -R UnitTests --output-on-failure
```

Run the fio demo (requires fio installed, or use the bundled fio with CMake variable override):

```bash
# Option A: Use CMake demo target and let it auto-detect (or auto-build) bundled fio
cmake --build build --target run_fio_demo -j1

# Option B: Manual run using system fio (set env for macOS)
# From the repo root:
ENGINE=$(pwd)/build/libssdsim_engine.so
export SSD_SIM_LIB_PATH="$(pwd)/build/libssdsim.dylib"
export DYLD_LIBRARY_PATH="$(pwd)/build:$(pwd)/build/_deps/systemc-build/src"
fio \
  --ioengine=external:"${ENGINE}" \
  --filename=$(pwd)/config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5

# Option C: Force a specific fio for the CMake demo target
# From your build directory:
cmake -DFIO_EXE_OVERRIDE=/usr/local/bin/fio -S .. -B .
cmake --build . --target run_fio_demo -j1
```

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

Notes on macOS:

- The CMake `run_fio_demo` target automatically sets SSD_SIM_LIB_PATH and DYLD_LIBRARY_PATH so the fio engine can find `libssdsim` and SystemC.
- For manual runs, set SSD_SIM_LIB_PATH and DYLD_LIBRARY_PATH as shown above.


Examples:

```bash
# Reconfigure build with custom params (from your build dir)
cmake -DDEMO_RW=randread -DDEMO_BS=16k -DDEMO_IODEPTH=16 -S .. -B .
cmake --build . --target run_fio_demo -j1

# Pin a specific fio binary
cmake -DFIO_EXE_OVERRIDE=/usr/local/bin/fio -S .. -B .
cmake --build . --target run_fio_demo -j1
```

### Two demo entry points: what and why

- run_fio_demo (CMake target)
  - Purpose: A developer-friendly, parameterized demo you run via your build tool.
  - How: `cmake --build <build_dir> --target run_fio_demo`
  - Notes: Uses the DEMO_* variables and optional `FIO_EXE_OVERRIDE`.

- FioDemoShort (CTest, optional)
  - Purpose: A super-fast smoke test for the end-to-end fio→engine→sim path.
  - How: Enable during configure, then run with ctest.
    - `cmake -DTOYSSD_DEMO_TEST=ON -S . -B build`
    - `(cd build && ctest -R FioDemoShort --output-on-failure)`
  - Defaults: 1s runtime, 4M total size. Disabled by default to keep unit tests lean.
  - Note: When `TOYSSD_DEMO_TEST` is ON, the CMake configure step prints a status line confirming registration (e.g., "Registering 'FioDemoShort' (1s, 4M)").

CI runs both: it enables `TOYSSD_DEMO_TEST=ON` to run `FioDemoShort`, then builds and runs `run_fio_demo` for a longer demonstration.

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

Option B — directly with clang-format:

```bash
# Show version
clang-format --version

# Format in-place (tracked files only; skips build artifacts and deps)
git ls-files '**/*.[ch]' '**/*.[ch]pp' ':!:build*/*' ':!:_deps/*' | xargs -r clang-format -i

# Check for diffs (non-zero exit on violations)
git ls-files '**/*.[ch]' '**/*.[ch]pp' ':!:build*/*' ':!:_deps/*' | xargs -r clang-format --dry-run --Werror
```

Notes:

- If `clang-format` isn’t installed, CMake’s `format`/`format-check` targets become no-ops and print a hint.
- On macOS, install via Homebrew: `brew install clang-format`.
- The build also depends on `format`, so running `cmake --build` will format sources automatically when `clang-format` is available.
