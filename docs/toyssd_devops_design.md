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
- [Dockerfile](#dockerfile)
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
- Docker image pre-bakes a uv-managed Python environment for tooling so containers don’t re-install on each run; dependency changes are handled by rebuilding the image.
- We do not commit a `uv.lock` to the repo by default to allow newer tool versions automatically; CI publishes a snapshot artifact (lock + versions manifest) each run for traceability.

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
- check – runs static checks only (no build/tests): cpp_format_check, cpp_lint, py_lint, py_typecheck.
- verify – full validation: cpp_configure → check → cpp_build → cpp_test.

Note: `verify` first configures CMake (to generate format/lint CTest targets), then runs checks, then builds and runs unit tests. Static analysis via clang-tidy is available separately through `invoke cpp_analyze` and isn’t part of `check` by default.

### Backend strategy (native vs docker)

Adopt a single-source-of-truth task set that’s parameterized by a backend flag (`native` or `docker`), plus thin convenience aliases prefixed with `docker_`. This avoids logic duplication while keeping commands discoverable.

- Core parameterized tasks: accept `backend` and `build_type` and derive the build directory.
- Aliases: `docker_cpp_*` call core tasks with `backend="docker"`.
- Aggregators: read default backend from `c.config.toyssd.backend` or `TOYSSD_BACKEND` env variable.
  
  For the Docker backend, Python tooling is executed via `uv run` against a pre-baked virtual environment inside the image (see Dockerfile). We set `UV_PROJECT_ENVIRONMENT=/opt/toyssd/.venv` so `uv run` uses that environment without attempting to install on the bind-mounted source tree. If dependencies change, rebuild the image rather than syncing at runtime.

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

### Unified matrix workflow

The project uses a single workflow at `.github/workflows/ci.yml` with a strategy matrix over `ubuntu-latest` and `macos-14`. Both platforms run the same stages to keep parity:

- Verify: `uv run invoke verify` (configure → static checks → build → CTest unit tests)
- Demo: `ctest --test-dir build-debug -L demo --output-on-failure`
- Coverage: `cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DENABLE_CODE_COVERAGE=ON` then `cmake --build build-coverage --target coverage`

Standardized artifacts for each OS:

- `test-results-${{ runner.os }}` → `build-debug/test-results/gtest/**/*.xml`
- `coverage-html-${{ runner.os }}` → `build-coverage/coverage/`
- `deps-snapshot-${{ github.sha }}-${{ runner.os }}` → `uv.lock`, `uv-requirements.txt`, `tool-versions.txt`

Linux runs inside the `toyssd` Docker image for reproducibility; macOS uses native runners via Homebrew-installed tooling.

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
uv run invoke cpp-build
uv run invoke cpp-test
```

## Dockerfile

We use a slim Ubuntu 24.04 base image that installs only minimal system dependencies with APT, installs `uv`, and then pre-installs the project’s Python tooling into an image-local virtual environment.

Key points:

- APT installs compiler toolchain and system libraries only (cmake, build-essential, clang-tools, libaio, zlib, numa, fio, etc.).
- `uv` is installed globally and used to create a persistent virtual environment at `/opt/toyssd/.venv` via `uv sync` during image build.
- Only dependency metadata (`pyproject.toml`, and optionally `uv.lock`) is copied into the image at build time. No project source code is copied. At runtime, the repository is bind-mounted at `/src`.
- `UV_PROJECT_ENVIRONMENT=/opt/toyssd/.venv` is set so `uv run` from within the container uses the pre-baked environment rather than creating or modifying environments on the bind mount.
- If you prefer strict pinning, add a `uv.lock`, copy it in the Dockerfile, and use `uv sync --frozen` for reproducible, cacheable builds. When the lock or `pyproject.toml` changes, rebuild the image to update dependencies. Default policy here is to omit `uv.lock` and allow latest-compatible resolution at build time; CI publishes a snapshot for traceability.
- Runtime containers should not attempt to install/update Python packages; if they would, rely on the existing `UV_PROJECT_ENVIRONMENT` and optionally set `UV_NO_SYNC=1` to avoid accidental network or writes.

Developer ergonomics:

- Run CMake/CTest commands directly in the container with your host UID/GID to avoid root-owned files.
- Run Python tooling through `uv run` inside the container; it will use the pre-baked environment without touching the bind-mounted worktree.

## VS Code tasks and docs

This project supports two parallel developer flows in VS Code:

- Fast local iteration with the built-in CMake Tools tasks (Debug-only by default)
- Full cross-platform automation with `uv run invoke ...` tasks (native or Docker backends)

### CMake Tools tasks (quick local builds)

We keep the existing CMake tasks provided by VS Code/CMake Tools for a quick Debug loop. These map to the following commands and use the `build-debug` directory:

- "Configure (Debug)" → `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug`
- "Build (Debug)" → `cmake --build build-debug -j`
- "Configure + Build (Debug)" → runs both in sequence

Notes:

- These tasks are meant for quick local iterations and target Debug builds only. For Release builds, testing, static analysis, or Docker parity, prefer the Invoke tasks below.
- Test execution from VS Code is easiest via the CTest integration: after building, run tests in `build-debug`.

### Invoke tasks (native and Docker)

All higher-level operations are standardized via Invoke. Run them from the VS Code integrated terminal using the pre-configured Python environment (via `uv`). Examples:

- Native (default backend):
  - `uv run invoke verify` — static checks + C++ build + CTest
  - `uv run invoke check` — static checks only (no build/tests)
  - `uv run invoke cpp_configure --build-type=Debug` → `build-debug`
  - `uv run invoke cpp_build --build-type=Debug`
  - `uv run invoke cpp_test --build-type=Debug`

- Docker backend (Ubuntu image):
  - Set `TOYSSD_BACKEND=docker` or use aliases like `uv run invoke docker_cpp_build`.
  - Docker builds use separate directories to avoid host/container mixing.

Build directory naming convention:

- VS Code CMake tasks: `build-debug` (Debug)
- Native (Invoke): `build-debug` (Debug), `build-release` (Release)
- Docker (Invoke): `build-docker-debug` (Debug), `build-docker-release` (Release)

Tip: You can change the default backend by setting `TOYSSD_BACKEND` or via `c.config.toyssd.backend` in Invoke's config. See the README for a quick reference.

## Decisions and Rationale

- Python package layout
  - We will create `python/toyssd/` and migrate scripts from `tools/` into that package (likely `python/toyssd/cli/`). This enables packaging and a clean `console_scripts` entry point.

- Dependency management
  - Fully migrate to `uv` + `pyproject.toml`; remove `requirements.txt` and any global `pip` installs from Docker.

- CI strategy
  - Use a single matrix workflow (`ci.yml`) for Ubuntu (Docker-based) and macOS (native).
  - Standardize artifacts and steps (verify, demo, coverage) across both OSes.

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
