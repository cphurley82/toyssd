# toyssd

[![ci](https://github.com/cphurley82/toyssd/actions/workflows/ci.yml/badge.svg)](https://github.com/cphurley82/toyssd/actions/workflows/ci.yml)
[![Agent rules](https://img.shields.io/badge/Agent%20rules-read-blue)](docs/agent_rules.md)

A modular, **SystemC/TLM**-based SSD simulator scaffold that integrates with **fio**, uses **GoogleTest** for TDD, and ships with **Docker** + **GitHub Actions CI**.  
Authored by **Chris Hurley**. Licensed under **MIT**.

Note on C++ standard: The project defaults to C++20 (no GNU extensions). You can opt into a newer standard by configuring with `-DCMAKE_CXX_STANDARD=23` (or higher, if supported by your toolchain).

## Contributing

Before opening a PR or using an AI agent, please read the short guidelines in [docs/agent_rules.md](docs/agent_rules.md).

## Quick Start (the easy way)

We standardize on uv + Invoke for all local and CI automation. Start here for the simplest experience.

### Native (macOS/Linux)

```bash
# One-time: install dev tools into .venv/
uv sync --all-extras

# Full validation: static checks + C++ build + tests
uv run invoke verify

# Common tasks
uv run invoke check          # static checks only (no build/tests)
uv run invoke cpp_build      # build C++ (Debug by default)
uv run invoke cpp_test       # run CTest (excludes demo by default)
uv run invoke py_format      # format Python
uv run invoke py_lint        # lint Python
uv run invoke py_typecheck   # mypy
```

Build directories (by convention): `build-debug` (Debug), `build-release` (Release).

### Docker (for Linux parity)

```bash
# Build the image once
docker build -t toyssd -f Dockerfile .

# Run the unified flow inside the container (uses pre-baked Python env)
docker run --rm -t --user "$(id -u)":"$(id -g)" -e HOME=/src -v "$PWD":/src -w /src toyssd \
  uv run invoke verify
```

Tip: You can also run from your host with a Docker backend by setting:

```bash
TOYSSD_BACKEND=docker uv run invoke verify
```

Docker build directories: `build-docker-debug` (Debug), `build-docker-release` (Release).

## Optional: VS Code

Using the CMake Tools extension, the default UI (status bar or Command Palette) configures to `build` by default:

- Configure → generates `${workspaceFolder}/build`
- Build → builds `${workspaceFolder}/build`

Tip: If you prefer a different build folder (e.g., `build-debug`), set `cmake.buildDirectory` in your VS Code settings. Use the Invoke tasks for full validation, Release builds, and Docker parity.

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

### Build directory naming

To keep native and Docker workflows clean and predictable, we use explicit build directories:

- VS Code CMake Tools (default UI): `build`
- Native (via `uv run invoke ...`):
  - Debug → `build-debug`
  - Release → `build-release`
- Docker (via `uv run invoke ...` with `TOYSSD_BACKEND=docker` or `docker_` aliases):
  - Debug → `build-docker-debug`
  - Release → `build-docker-release`

Use CMake Tools for quick local iterations; use Invoke tasks for full validation (checks, tests) and for Docker parity.

## Formatting & linting

Prefer the Invoke tasks:

```bash
uv run invoke cpp_format        # clang-format (in-place)
uv run invoke cpp_format_check  # verify formatting
uv run invoke py_format         # Ruff format
uv run invoke py_lint           # Ruff lint
uv run invoke py_typecheck      # mypy
```

Config files: `.clang-format`, `pyproject.toml`.

---

License: MIT (see `LICENSE`).

## Troubleshooting (see docs for more)

- clang-tidy not found: install llvm and ensure `$(brew --prefix llvm)/bin` (macOS) or `clang-tidy` (Linux) is on your PATH, or pass `-DCLANG_TIDY_EXE=...` at configure.
- Prefer bundled vs system fio: for reproducible runs, use the CTest demo. For manual experimentation, see docs.

## Python tooling (uv + Invoke)

This repo uses [uv](https://github.com/astral-sh/uv) for fast Python env management and [Invoke](https://www.pyinvoke.org/) for tasks. Dev dependencies include cpplint, ruff, mypy, pytest, and gcovr.

### Tasks quick reference

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

### CI expectations (summary)

Single workflow `ci.yml` runs on a matrix of macOS and Linux:

- Verify: `uv run invoke verify` (configure → static checks → build → unit tests)
- Demo: `ctest --test-dir build-debug -L demo --output-on-failure`
- Coverage: `cmake --build build-coverage --target coverage` (HTML report under `build-coverage/coverage/`)
- Artifacts (standardized names):
  - `test-results-${{ runner.os }}` → `build-debug/test-results/gtest/**/*.xml`
  - `coverage-html-${{ runner.os }}` → `build-coverage/coverage/`
  - `deps-snapshot-${{ github.sha }}-${{ runner.os }}` → `uv.lock`, `uv-requirements.txt`, `tool-versions.txt`

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

### Python API

The Python package exposes a `ToySSD` façade that instantiates the full SystemC top module (host interface, firmware, and NAND). Build the project once via CMake (or `uv run invoke verify`), then interact from Python:

```bash
uv pip install -e .  # editable install for development

uv run python - <<'PY'
from toyssd import ToySSD

sim = ToySSD()
req_id = sim.submit_write(0, 4096)
completions = []
while not completions:
    completions.extend(sim.poll(4))

for event in sim.drain_nand_events():
    print(event)
PY
```

Optional visualization helpers (pandas/matplotlib) remain under the `viz` extra:

```bash
uv sync --extra viz
```

## Agent workflow (local rules)

- Run the full verification step after each substantive change: `uv run invoke verify`.
- In VS Code, use the workspace tasks: “Verify” (default Test task) or “Build + Verify (Debug)”.

See also: `docs/agent_rules.md` for the detailed expectations followed by agents and contributors.

## Manual fio run (advanced)

Prefer the CTest-driven demo (see CLI helper above) because it wires all the env vars automatically. If you want to run fio manually against the external ioengine, set a couple of environment variables and pass the ioengine module path explicitly.

macOS (Darwin):

```bash
# Adjust these absolute paths to your build tree
export SSD_SIM_LIB_PATH="/abs/path/to/libssdsim.dylib"
export DYLD_LIBRARY_PATH="/abs/path/to/dir-containing-libssdsim:/abs/path/to/systemc/libdir"

fio \
  --ioengine=external:/abs/path/to/libssdsim_engine.dylib \
  --filename=config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 \
  --time_based --runtime=1
```

Linux:

```bash
# Adjust these absolute paths to your build tree
export SSD_SIM_LIB_PATH="/abs/path/to/libssdsim.so"
export LD_LIBRARY_PATH="/abs/path/to/dir-containing-libssdsim:/abs/path/to/systemc/libdir"

LD_PRELOAD="/abs/path/to/libscmain_stub.so" fio \
  --ioengine=external:/abs/path/to/libssdsim_engine.so \
  --filename=config/default.json \
  --name=demo --rw=randwrite --size=64M --bs=4k --iodepth=8 \
  --time_based --runtime=1
```

Tips

- Artifacts usually live under your build directory (e.g., `build-debug/Debug/lib` on multi-config generators). Use `find build-debug -name 'libssdsim*'` to locate them.
- If fio fails to load the engine, verify the dynamic library paths (`DYLD_LIBRARY_PATH` on macOS, `LD_LIBRARY_PATH` on Linux) include both the directory of `libssdsim` and the SystemC library directory.
- The CTest demo target sets these automatically; for a quick sanity check, prefer running it first.

## Documentation index

- [docs/toyssd_devops_design.md](docs/toyssd_devops_design.md) — DevOps workflow and tooling
  - What: End-to-end build/test/lint/format strategy, uv + Invoke tasks, Docker parity.
  - Who: All contributors; CI maintainers.
- [docs/toy_ssd_simulator_design.md](docs/toy_ssd_simulator_design.md) — Architecture and phased plan
  - What: System overview, modules (HostInterface, Firmware/FTL, NAND), outside-in roadmap.
  - Who: Simulator developers and researchers.
- [docs/nand_checker_design.md](docs/nand_checker_design.md) — NAND checker framework (design-only for now)
  - What: Catalog of runtime validation checkers (geometry, protocol ordering, timing, etc.).
  - Who: Firmware/verification engineers; future implementers.
- [docs/coverage_plan.md](docs/coverage_plan.md) — Coverage goals and next steps
  - What: Plan to increase unit test coverage and notes from a dead code review.
  - Who: Test authors and reviewers.
- [docs/TODO.md](docs/TODO.md) — Backlog
  - What: Project backlog, CI enhancements, API hardening, packaging.
  - Who: Maintainers/PMs.
