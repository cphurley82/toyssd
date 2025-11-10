# Coverage Design

## Purpose

Define a simple, reproducible coverage setup for both C++ (SystemC core) and Python that:

- Works locally on macOS and inside the Linux Docker dev image/CI.
- Uses a dedicated build tree at `build/coverage` to keep other builds clean.
- Generates separate reports for C++ and Python under `coverage/` with low ceremony.
- Adds a single `invoke coverage` entrypoint that runs both and writes artifacts.

This document records the implemented approach so future changes stay consistent.

## Overview

- Build tree: `build/coverage`
- Artifacts: `coverage/cpp/` and `coverage/python/`
- Task: `invoke coverage` (see [tools/invoke_tasks.py](../tools/invoke_tasks.py))
- CI: Run coverage only on the Linux runner; gate on 60% C++ line coverage and 60% Python line coverage. Increase thresholds as coverage improves.

## C++ Coverage (CTest/GoogleTest)

Tooling and flags

- Instrumentation: GCC/Clang-compatible `--coverage -O0 -g` for compile and link.
- On Debian-based images (Docker/devcontainer/CI) install `libclang-rt-14-dev` for the profiling runtime and `llvm-14` so `llvm-cov gcov` is available for report generation.
- Reporter: `gcovr` to produce HTML details and Cobertura-format XML.
- Exclusions: tests, examples, third_party, the preinstalled SystemC tree (e.g., `/opt/systemc`), and the entire `_deps/` tree to drop vendored third-party sources from the denominator.

CMake toggle

- `TOYSSD_ENABLE_COVERAGE` in [CMakeLists.txt](../CMakeLists.txt) appends `--coverage -O0 -g` to compile/link options when enabled. The toggle is only set inside the `build/coverage` tree so other configurations remain untouched.

Build and run (manual example)

```bash
cmake -S . -B build/coverage -DCMAKE_BUILD_TYPE=Debug -DTOYSSD_ENABLE_COVERAGE=ON
cmake --build build/coverage --parallel
ctest --test-dir build/coverage --output-on-failure

# Generate reports
mkdir -p coverage/cpp
gcovr -r . --object-directory build/coverage \
  --html-details -o coverage/cpp/index.html \
  --xml-pretty -o coverage/cpp/cobertura.xml \
  --branches \
  --exclude '.*(tests|examples|third_party|/opt/systemc).*'
```

Note on macOS (Apple Clang)
- Apple Clang supports `--coverage`, but `gcovr` may need LLVM’s gcov frontend.
- Use: `gcovr --gcov-executable "llvm-cov gcov" ...` when running on macOS.

## Python Coverage (pytest)

Tooling

- `pytest`, `pytest-cov`, `coverage` (configured in [pyproject.toml](../pyproject.toml)).

Run (manual example)

```bash
uv run pytest -q \
  --cov=toyssd --cov-branch \
  --cov-report=xml:coverage/python/coverage.xml \
  --cov-report=html:coverage/python/html
```

Configuration

- In [pyproject.toml](../pyproject.toml), configure coverage to omit tests, generated code, and examples where appropriate.

## Invoke Task: `coverage`

`invoke coverage` (in [tools/invoke_tasks.py](../tools/invoke_tasks.py)):
- Configures `build/coverage` with `-DTOYSSD_ENABLE_COVERAGE=ON`, builds, and runs `ctest`.
- Runs `gcovr` with the exclusions above to produce HTML + Cobertura-format reports in `coverage/cpp/`.
- Runs `pytest --cov` to emit HTML + XML reports in `coverage/python/`.
- On macOS the task forces `gcovr --gcov-executable "llvm-cov gcov"` to use LLVM’s gcov shim.
- Optional thresholds are exposed via `--fail-under-cpp` and `--fail-under-python` flags (defaults: no gate locally).

## CI Integration

Policy

- Run coverage only on Linux to save CI minutes and avoid toolchain variance.
- Use realistic-but-achievable gates (currently 60% C++ lines / 60% Python lines) and ratchet upward as we add tests.

Implementation (GitHub Actions)
- Job `docker-coverage` in [.github/workflows/ci.yml](../.github/workflows/ci.yml) runs on `ubuntu-latest` inside the dev container.
- Steps: checkout → build container → `invoke bootstrap` + `invoke coverage --fail-under-cpp 60 --fail-under-python 60`.
- Artifacts `coverage/cpp/` and `coverage/python/` are uploaded for inspection.
- macOS jobs skip coverage to avoid redundant `gcovr` runs and to match the user requirement that Linux CI is the single source for coverage gating.

## Directory Layout

```text
build/
  coverage/              # CMake build tree for coverage
coverage/
  cpp/
    index.html           # HTML details report
    cobertura.xml        # Cobertura-format XML (CI-friendly)
  python/
    html/                # HTML report
    coverage.xml         # Coverage XML (CI-friendly)
```

## Rationale & Best Practices

- Separate build trees avoid polluting Debug/ASAN/Release with instrumentation.
- Prefer line + branch coverage where supported; exclude third-party and SystemC headers.
- Keep compiler optimizations off (`-O0`) and include debug info (`-g`) for accurate mapping.
- Maintain 60%/60% gates so regressions fail fast, and increase them as coverage grows.

## Status

The coverage flow is fully wired (CMake option, Invoke task, docs, and CI job). Future changes should keep the dedicated `build/coverage` tree, separate reports, Linux-only CI gating, and revisit the 60%/60% gates as the suite improves.
