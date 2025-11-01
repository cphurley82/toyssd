# toyssd DevOps Design

This document outlines a consistent DevOps workflow for the toyssd project across macOS, Linux, CI, and Docker, covering build, test, lint, and formatting for both C++ and Python.

## Contents

- [Goals](#goals)
- [Toolchain Overview](#toolchain-overview)
- [System Requirements](#system-requirements)
  - [macOS (Homebrew)](#macos-homebrew)
  - [Ubuntu 24.04 (APT)](#ubuntu-2404-apt)
  - [Python dependencies](#python-dependencies)
- [Python Packaging and Tooling](#python-packaging-and-tooling)
- [Invoke Tasks Overview](#invoke-tasks-overview)
- [CMake Integration Highlights](#cmake-integration-highlights)
- [CI Workflow (GitHub Actions)](#ci-workflow-github-actions)
- [Developer Workflow](#developer-workflow)
- [TODO (Migration Plan)](#todo-migration-plan)
- [Decisions and Rationale](#decisions-and-rationale)
- [Summary](#summary)

## Goals

- Unified workflow: `uv run invoke <task>` works everywhere (macOS, Linux, CI, Docker).
- Minimal global dependencies: only **uv**, **CMake**, compiler (**clang/AppleClang**), **ninja/make**, optional **Docker**.
- CI performs **clang-format**, **cpplint**, **clang-tidy**, **Ruff lint/format**, **mypy**, and unit tests.
- macOS CI runs for free where possible.
- SystemC is built from source via CMake.
- Python and C++ both conform to consistent lint/format rules.

## Toolchain Overview

| Area                   | Tool                                   | Notes                                                                  |
|------------------------|----------------------------------------|------------------------------------------------------------------------|
| Python env & deps      | uv                                      | Manages virtualenvs, installs dependencies quickly.                    |
| Task orchestration     | Invoke                                  | tasks.py defines build/test/lint flows.                                |
| C++ build              | CMake                                   | Main build system, integrates clang-format and clang-tidy.             |
| C++ lint & format      | clang-format, clang-tidy, cpplint       | Style, lint, static analysis in CI.                                    |
| Python lint & format   | Ruff, mypy                              | Fast linting/formatting plus static type checks.                       |
| Tests                  | GTest + CTest                           | Unit tests with JUnit XML output for CI.                               |
| Containers             | Docker                                  | Ubuntu 24.04 base image for parity.                                    |
| CI                     | GitHub Actions                          | Ubuntu and macOS runners.                                              |

## System Requirements

### macOS (Homebrew)

```bash
brew install uv cmake ninja llvm clang-format clang-tidy
```

### Ubuntu 24.04 (APT)

```bash
sudo apt-get update
sudo apt-get install -y uv cmake ninja-build build-essential clang clang-format clang-tidy git curl
```

### Python dependencies

Creates a local virtual environment in `.venv/` and installs development dependencies.

```bash
uv sync --all-extras
```

## Python Packaging and Tooling

**pyproject.toml**:

```toml
[build-system]
requires = ["hatchling>=1.25"]
build-backend = "hatchling.build"

[project]
name = "toyssd"
version = "0.1.0"
description = "Toy SSD simulator (C++/SystemC) with Python packaging interface"
readme = "README.md"
requires-python = ">=3.10"
license = { text = "MIT" }
authors = [{ name = "Your Name" }]

[project.optional-dependencies]
dev = [
  "invoke>=2.2",
  "build>=1.2",
  "pytest>=8.0",
  "rich>=13.0",
  "cpplint>=1.6.1",
  "ruff>=0.6.0",
  "mypy>=1.10"
]

[tool.ruff]
src = ["python", "tests"]
line-length = 100
target-version = "py310"

[tool.ruff.lint]
select = ["E","F","W","I","B","UP","SIM","FBT","A","C4","DTZ"]

[tool.ruff.format]
quote-style = "double"
indent-style = "space"

[tool.mypy]
python_version = "3.10"
packages = ["toyssd"]
strict = false
ignore_missing_imports = true
warn_unused_ignores = true
```

## Invoke Tasks Overview

Task names and descriptions (C++ tasks are CMake/CTest wrappers):

- env_check – verify required tools exist and are on PATH.
- cpp_configure, cpp_build, cpp_test – orchestrate CMake config/build and CTest.
- cpp_format, cpp_format_check – wrappers that run the CMake targets `format` and `format-check`.
- cpp_lint – wrapper that runs the CTest test `cpplint` (if enabled by CMake configure).
- cpp_analyze – wrapper flow that configures with `-DTOYSSD_ENABLE_CLANG_TIDY=ON` and builds to run clang-tidy via CMake.
- py_format – Ruff format.
- py_lint – Ruff lint.
- py_typecheck – mypy type checks.
- check – runs all static checks only (no build/tests): cpp_format_check, cpp_lint, cpp_analyze, py_lint, py_typecheck.
- verify – full validation: check + cpp_build + cpp_test.

Example Python tasks (C++ tasks call CMake/CTest under the hood):

```python
@task
def py_format(c):
  c.run("uv run ruff fmt python tests", pty=True)

@task
def py_lint(c):
    c.run("uv run ruff check python tests", pty=True)

@task
def py_typecheck(c):
  c.run("uv run mypy", pty=True)

@task(pre=[cpp_format_check, cpp_lint, cpp_analyze, cpp_test, py_lint, py_typecheck])
def verify(c):
    pass
```

### Backend strategy (native vs docker)

Adopt a single-source-of-truth task set that’s parameterized by a backend flag (`native` or `docker`), plus thin convenience aliases prefixed with `docker_`. This avoids logic duplication while keeping commands discoverable.

- Core parameterized tasks: accept `backend` and `build_type` and derive the build directory.
- Aliases: `docker_cpp_*` call core tasks with `backend="docker"`.
- Aggregators: read default backend from `c.config.toyssd.backend` or `TOYSSD_BACKEND` env variable.

Sketch:

```python
from invoke import task

def build_dir_for(backend: str, build_type: str) -> str:
  if backend == "docker":
    return "build-docker-debug" if build_type == "Debug" else "build-docker-release"
  return "build-debug" if build_type == "Debug" else "build-release"

def get_backend(c, override=None):
  if override:
    return override
  cfg = getattr(c, "config", None)
  if cfg and hasattr(cfg, "toyssd") and isinstance(cfg.toyssd, dict) and "backend" in cfg.toyssd:
    return cfg.toyssd["backend"]
  import os
  return os.environ.get("TOYSSD_BACKEND", "native")

def run_cmd(c, cmd: str, backend: str, image: str = "toyssd"):
  if backend == "native":
    return c.run(cmd, pty=True)
  # docker
  repo = c.run("pwd", hide=True).stdout.strip()
  uid = c.run("id -u", hide=True).stdout.strip()
  gid = c.run("id -g", hide=True).stdout.strip()
  docker_cmd = (
    f'docker run --rm -t --user "{uid}:{gid}" -e HOME=/src '
    f'-v "{repo}":/src -w /src {image} {cmd}'
  )
  return c.run(docker_cmd, pty=True)

@task
def cpp_configure(c, build_type="Debug", backend=None, werror=True, tidy=False):
  backend = get_backend(c, backend)
  bdir = build_dir_for(backend, build_type)
  flags = [
    f"-DTOYSSD_ENABLE_WERROR={'ON' if werror else 'OFF'}",
    f"-DTOYSSD_ENABLE_CLANG_TIDY={'ON' if tidy else 'OFF'}",
  ]
  cmd = f"cmake -S . -B {bdir} -DCMAKE_BUILD_TYPE={build_type} " + " ".join(flags)
  run_cmd(c, cmd, backend)

@task
def cpp_build(c, build_type="Debug", backend=None):
  backend = get_backend(c, backend)
  bdir = build_dir_for(backend, build_type)
  run_cmd(c, f"cmake --build {bdir} -j", backend)

@task
def cpp_test(c, build_type="Debug", backend=None):
  backend = get_backend(c, backend)
  bdir = build_dir_for(backend, build_type)
  run_cmd(c, f"ctest --test-dir {bdir} --output-on-failure -LE demo", backend)

# Aliases for ergonomics in CI/CLI
@task
def docker_cpp_configure(c, build_type="Debug", werror=True, tidy=False):
  cpp_configure(c, build_type=build_type, backend="docker", werror=werror, tidy=tidy)

@task
def docker_cpp_build(c, build_type="Debug"):
  cpp_build(c, build_type=build_type, backend="docker")

@task
def docker_cpp_test(c, build_type="Debug"):
  cpp_test(c, build_type=build_type, backend="docker")
```

## CMake Integration Highlights

- Treat warnings as errors: `-DTOYSSD_ENABLE_WERROR=ON`
- clang-format targets (`format`, `format-check`)
- clang-tidy optional (`-DTOYSSD_ENABLE_CLANG_TIDY=ON`)
- SystemC built via FetchContent (`SystemC.cmake`)

## CI Workflow (GitHub Actions)

### GitHub Actions workflows

We will keep the existing Docker-based workflow for Ubuntu (build, unit-tests, demo, coverage) and add a second workflow to validate on macOS using native runners and `uv`.

Example macOS workflow: `.github/workflows/macos-ci.yml`

```yaml
name: macos-ci
on: [push, pull_request]

jobs:
  build-test-macos:
    runs-on: macos-14
    steps:
      - uses: actions/checkout@v4
      - name: Install system deps
        run: |
          brew update
          brew install uv cmake ninja llvm clang-format clang-tidy
      - name: Install Python deps
        run: uv sync --all-extras
      - name: Build + Test (native)
        run: uv run invoke verify
      - name: C++ Lint/Format/Analyze
        run: |
          uv run invoke cpp_format_check
          uv run invoke cpp_lint
          uv run invoke cpp_analyze
      - name: Python Lint/Format/Types
        run: |
          uv run invoke py_format
          uv run invoke py_lint
          uv run invoke py_typecheck
      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results-macos
          path: build-debug/test-results/gtest/**/*.xml

Note: The existing Docker-based workflow files remain (e.g., `build.yml` with build/test/demo/coverage jobs). Over time, we may consolidate, but the immediate plan is to keep Docker-based CI for Linux parity and fio demos while adding macOS native coverage.
```

## Developer Workflow

```bash
# Install tools
uv sync --all-extras

# Full local verify (checks + build + tests)
uv run invoke verify

# Format Python code
uv run invoke py_format

# Lint Python code
uv run invoke py_lint

# Build + Test C++
uv run invoke cpp_build
uv run invoke cpp_test
```

## TODO (Migration Plan)

This repository already has a solid CMake/CTest foundation, Docker for reproducible builds, and CI that builds/tests in Docker and runs clang-format checks. To fully align with the new design, implement the following high-level steps:

- Python environment and tooling
  - [x] Add `pyproject.toml` (MIT license) with `dev` extras: invoke, pytest, rich, cpplint, ruff, mypy, gcovr.
  - [x] Create a proper Python package at `python/toyssd/` and migrate scripts from `tools/` into `python/toyssd/cli/` (or similar). Provide a `console_scripts` entry point (e.g., `toyssd=toyssd.cli:main`).
  - [x] Adopt `uv` end-to-end: ensure `.venv/` is in `.gitignore`, document `uv sync --all-extras` and `uv run ...` in `README.md`.
  - [x] Remove `requirements.txt` usage and switch Docker/CI to `uv` for Python tooling installation (a deprecated stub file remains temporarily for transition).
  - [x] Add `tasks.py` (Invoke) using parameterized core tasks with a `backend` flag (`native`/`docker`) and small `docker_*` alias tasks. Ensure aggregator tasks (e.g., `verify`, `check`) honor `c.config.toyssd.backend` or `TOYSSD_BACKEND`.

- CMake integration
  - [ ] Add toggle `-DTOYSSD_ENABLE_WERROR=ON` (default ON) to treat warnings as errors; allow turning off locally. Apply only to project targets (`simlib`, `ssdsim`, tests, fio engine), not external deps.
  - [ ] Add optional clang-tidy integration `-DTOYSSD_ENABLE_CLANG_TIDY=ON`; set `CMAKE_CXX_CLANG_TIDY` only on our targets and restrict scope via `.clang-tidy` `HeaderFilterRegex: '^(api|sim|fio_plugin)/'`.
  - [ ] Keep existing `format` and `format-check` targets. Keep `cpplint` as a CTest (present today) and optionally add a `cpplint` custom target alias for convenience.
  - [ ] Ensure GoogleTest XML output remains under the selected build dir (e.g., `build-debug/test-results/gtest/`) for CI ingestion.

- CI (GitHub Actions)
  - [ ] Keep the existing Docker-based Ubuntu workflow (build/test/demo/coverage) unchanged for Linux parity and fio demos.
  - [ ] Add a new macOS workflow (`macos-ci.yml`) that installs toolchain + `uv`, runs `uv run invoke verify`, then runs lint/type checks via Invoke.
  - [ ] Upload GTest XML results from `build-debug/test-results/gtest/**/*.xml`.
  - [ ] Add an optional coverage job using `-DENABLE_CODE_COVERAGE=ON` + `coverage` target; keep threshold low (e.g., 20%). Install `gcovr` via `uv` dev extras.

- Dockerfile
  - [ ] Slim Dockerfile to install toolchain only, install `uv`, and rely on `uv run` in bind-mounted workspace for Python tooling (avoid global `pip`).
  - [ ] Remove `requirements.txt` usage and associated `pip install` steps.
  - [ ] Document container usage with `uv run` equivalents in the README.

- VS Code tasks and docs
  - [ ] Keep existing CMake tasks for quick local workflows (`build/` only).
  - [ ] Document Invoke tasks and the build directory naming convention below.
  - [ ] Update `README.md` to reflect the unified `uv run invoke ...` workflow, native vs Docker flows, and the new CI expectations.

## Decisions and Rationale

- Python package layout
  - We will create `python/toyssd/` and migrate scripts from `tools/` into that package (likely `python/toyssd/cli/`). This enables packaging and a clean `console_scripts` entry point.

- Dependency management
  - Fully migrate to `uv` + `pyproject.toml`; remove `requirements.txt` and any global `pip` installs from Docker.

- CI strategy
  - Keep the existing Docker-based Ubuntu workflow for reproducibility and fio demos.
  - Add a new macOS workflow using native runners and `uv` for parity across OSes.

- clang-tidy scope
  - Only run clang-tidy on our sources (api/, sim/, fio_plugin/). Exclude external deps (`_deps/`) and tests. Restrict via `.clang-tidy` HeaderFilterRegex and apply `CMAKE_CXX_CLANG_TIDY` only to our targets.

- Warnings-as-errors
  - Add `-DTOYSSD_ENABLE_WERROR` (default ON) and allow developers to disable locally as needed.

- Coverage
  - Install `gcovr` via `uv` dev extras; keep the threshold low initially (e.g., 20%). If using Clang, `gcovr` can operate via `llvm-cov gcov`.

- License
  - Standardize on MIT across `LICENSE`, Docker labels, and `pyproject.toml`.

- Build directory naming
  - Use: `build` (VS Code tasks only), `build-debug` and `build-release` (native via Invoke), and `build-docker-debug`/`build-docker-release` (Docker via Invoke).

### Native vs. Docker flows with Invoke

- Native builds (macOS/Linux):
  - `cpp_configure_debug` -> configures to `build-debug`
  - `cpp_build_debug` -> builds `build-debug`
  - `cpp_configure_release` -> configures to `build-release`
  - `cpp_build_release` -> builds `build-release`
  - `cpp_test` -> runs CTest in the selected native build dir (default `build-debug`)

- Docker builds (Ubuntu image):
  - `cpp_docker_configure_debug` -> configures to `build-docker-debug`
  - `cpp_docker_build_debug` -> builds `build-docker-debug`
  - `cpp_docker_configure_release` -> configures to `build-docker-release`
  - `cpp_docker_build_release` -> builds `build-docker-release`
  - `cpp_docker_test` -> runs CTest in the selected docker build dir

Each core Invoke task accepts `backend` (native/docker) and passes through options like `-DTOYSSD_ENABLE_WERROR` and `-DTOYSSD_ENABLE_CLANG_TIDY`. Alias tasks (`docker_cpp_*`) call the core with `backend="docker"`.

## Summary

- **uv** handles Python deps; **Invoke** unifies all automation.
- **CMake/CTest** remains canonical for C++ builds.
- **Ruff** + **mypy** enforce Python style & types.
- **clang-format**, **cpplint**, **clang-tidy** enforce C++ style.
- CI mirrors local tasks for identical checks.
- macOS and Ubuntu both validated in GitHub Actions.
- SystemC built from source for portability.
