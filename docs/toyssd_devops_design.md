# toyssd DevOps Design (C++ + Python Formatting/Linting)

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
license = { text = "Apache-2.0" }
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
src = ["python", "src", "tests"]
line-length = 100
target-version = "py310"

[tool.ruff.lint]
select = ["E","F","W","I","B","UP","SIM","FBT","A","C4","DTZ"]

[tool.ruff.format]
quote-style = "double"
indent-style = "space"

[tool.mypy]
python_version = "3.10"
packages = ["python"]
strict = false
ignore_missing_imports = true
warn_unused_ignores = true
```

## Invoke Tasks Overview

Task names and descriptions:

- env_check – verify required tools exist and are on PATH.
- cpp_configure, cpp_build, cpp_test – orchestrate CMake config/build and CTest.
- cpp_format, cpp_format_check – apply/check clang-format.
- cpp_lint – cpplint for style checks.
- cpp_analyze – clang-tidy static analysis.
- py_format – Ruff format.
- py_lint – Ruff lint.
- py_typecheck – mypy type checks.
- check – runs all static checks only (no build/tests): cpp_format_check, cpp_lint, cpp_analyze, py_lint, py_typecheck.
- verify – full validation: check + cpp_build + cpp_test.

Example Python tasks:

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

## CMake Integration Highlights

- Treat warnings as errors: `-DTOYSSD_ENABLE_WERROR=ON`
- clang-format targets (`format`, `format-check`)
- clang-tidy optional (`-DTOYSSD_ENABLE_CLANG_TIDY=ON`)
- SystemC built via FetchContent (`SystemC.cmake`)

## CI Workflow (GitHub Actions)

### GitHub Actions workflow: `.github/workflows/ci.yml`

```yaml
name: CI
on: [push, pull_request]

jobs:
  build-test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-24.04, macos-14]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Install system deps
        run: |
          if [[ $RUNNER_OS == "Linux" ]]; then
            sudo apt-get update
            sudo apt-get install -y cmake ninja-build build-essential clang clang-format clang-tidy git curl
            curl -LsSf https://astral.sh/uv/install.sh | sh
            echo "$HOME/.local/bin" >> $GITHUB_PATH
          else
            brew update
            brew install uv cmake ninja llvm clang-format clang-tidy
          fi
      - name: Install Python deps
        run: uv sync --all-extras
      - name: Build + Test
        run: uv run invoke all
      - name: C++ Lint/Format
        run: |
          uv run invoke format_check
          uv run invoke cpplint
          uv run invoke clang_tidy
      - name: Python Lint/Format/Types
        run: |
          uv run invoke py_fmt
          uv run invoke py_lint
          uv run invoke py_types
      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results-${{ matrix.os }}
          path: build/test-results/*.xml
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

## Summary

- **uv** handles Python deps; **Invoke** unifies all automation.
- **CMake/CTest** remains canonical for C++ builds.
- **Ruff** + **mypy** enforce Python style & types.
- **clang-format**, **cpplint**, **clang-tidy** enforce C++ style.
- CI mirrors local tasks for identical checks.
- macOS and Ubuntu both validated in GitHub Actions.
- SystemC built from source for portability.
