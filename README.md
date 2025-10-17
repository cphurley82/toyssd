# toy-ssd-sim

A modular, **SystemC/TLM**-based SSD simulator scaffold that integrates with **fio**, uses **GoogleTest** for TDD, and ships with **Docker** + **GitHub Actions CI**.  
Authored by **Chris Hurley**. Licensed under **MIT**.

## Quick Start (Docker)

The repo ships with a minimal Ubuntu 24.04 container that builds SystemC, the simulator, and unit tests. Python tools install into a virtualenv.

Build the image:

```bash
docker build -t toy-ssd-sim -f Dockerfile .
```

Run unit tests (filter to project tests to avoid SystemC example tests):

```bash
docker run --rm -t toy-ssd-sim -lc "cd build && ctest -R UnitTests --output-on-failure"
```

Run all tests (may include upstream SystemC example tests if present):

```bash
docker run --rm -t toy-ssd-sim -lc "cd build && ctest --output-on-failure"
```

Run the fio demo (Option A: use system fio inside the container):

```bash
# install fio (one-time in a derived container or an interactive shell)
docker run -it --rm toy-ssd-sim bash
apt-get update && apt-get install -y fio

# then from inside the container
cd /workspace/build
FIO_ENGINE=$(pwd)/fio_plugin/libssdsim_engine.so
fio \
  --ioengine=external:${FIO_ENGINE} \
  --filename=/workspace/config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5
```

Run the fio demo (Option B: via CMake target, if your environment has a valid fio binary path wired):

```bash
docker run --rm -t toy-ssd-sim -lc "cd build && cmake --build . --target run_fio_demo"
```

Note: If Option B fails due to a missing fio binary, use Option A or modify the CMake demo target to call a system fio.

---

## Native Build (macOS)

Prerequisites:

- Xcode Command Line Tools (or Xcode): `xcode-select --install`
- CMake 3.16+ and Git (e.g., via Homebrew: `brew install cmake git`)
- Optional: fio (for the demo): `brew install fio`

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

Run the fio demo (requires fio installed):

```bash
ENGINE=$(pwd)/fio_plugin/libssdsim_engine.so  # adjust if using a different build dir
fio \
  --ioengine=external:${ENGINE} \
  --filename=$(pwd)/../config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 --numjobs=1 \
  --time_based --runtime=5
```

## Repo Structure

- `api/` — C API exposed to fio ioengine
- `sim/` — SystemC simulator modules (host, firmware with FTL, NAND)
- `fio_plugin/` — fio ioengine code and demo job
- `tests/` — GoogleTest unit tests
- `tools/` — Python scripts for running/analyzing
- `docs/` — design document
- `config/` — sample NAND/controller config
- `.github/workflows/` — CI pipeline

See `docs/ssd_simulator_design.md` for full design details.
