# toyssd

A modular, **SystemC/TLM**-based SSD simulator scaffold that integrates with **fio**, uses **GoogleTest** for TDD, and ships with **Docker** + **GitHub Actions CI**.  
Authored by **Chris Hurley**. Licensed under **MIT**.

## Quick Start (Docker)

The repo ships with a minimal Ubuntu 24.04 container that builds SystemC, the simulator, and unit tests. Python tools install into a virtualenv.

Build the image:

```bash
docker build -t toyssd -f Dockerfile .
```

Run unit tests (filter to project tests to avoid SystemC example tests):

```bash
docker run --rm -t toyssd -lc "cd build && ctest -R UnitTests --output-on-failure"
```

Run all tests (may include upstream SystemC example tests if present):

```bash
docker run --rm -t toyssd -lc "cd build && ctest --output-on-failure"
```

Run the fio demo (Option A: via CMake target)

```bash
docker run --rm -t toyssd -lc "cd build && cmake --build . --target run_fio_demo"
```

This target auto-detects a fio binary bundled from sources during the CMake configure step. If your environment lacks fio, the target will fall back to the bundled one when available.

Run the fio demo (Option B: manual fio invocation inside the container):

```bash
# install fio (one-time in a derived container or an interactive shell)
docker run -it --rm toyssd bash

# Inside the container (fio is already built from source during CMake)
cd /workspace/build
FIO=$(pwd)/_deps/fio-src/fio
FIO_ENGINE=$(pwd)/libssdsim_engine.so
"${FIO}" \
  --ioengine=external:"${FIO_ENGINE}" \
  --filename=/workspace/config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5
```

Note: If the CMake target fails due to a missing fio binary, use the manual Option B above.

---

## Native Build (macOS)

Prerequisites:

- Xcode Command Line Tools (or Xcode): `xcode-select --install`
- CMake 3.16+ and Git (e.g., via Homebrew: `brew install cmake git`)
- Optional: fio (for the demo): `brew install fio`
  - Not required: if not installed, the build will use a bundled fio (built from sources) by default.

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
