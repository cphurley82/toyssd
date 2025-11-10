"""Invoke tasks for the toyssd development workflow.

This task collection standardizes common developer actions (bootstrap, build,
test, format, lint) across platforms. The script intentionally prefers
deterministic, reproducible configuration.

Every task runs commands with a merged environment that includes the necessary
compiler and SDK hints. Tasks are designed to be idempotent; re-running them
should be safe.
"""

from __future__ import annotations

import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from invoke import Collection, Exit, task

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"
COVERAGE_DIR = ROOT / "coverage"


def _detect_sdkroot() -> str | None:
    """Return an SDKROOT suitable for C/C++ builds on macOS, if available.

    Why: Apple toolchains rely on a macOS SDK that provides system headers and
    frameworks not found in the bare filesystem. When SDKROOT is set (either by
    the user's shell or by Xcode tools), compilers can locate the correct
    headers consistently. We prefer to explicitly set it to avoid environment
    drift between machines and CI runners.

    Strategy:
    - If not on macOS, do nothing.
    - If ``SDKROOT`` is already present in the environment, respect it.
    - Otherwise, ask ``xcrun`` for the active macOS SDK path.
    - On failure (e.g., xcode-select not configured), return ``None`` and let
      CMake/Clang use their defaults.
    """
    if platform.system() != "Darwin":
        return None
    sdkroot = os.environ.get("SDKROOT")
    if sdkroot:
        return sdkroot
    try:
        return subprocess.check_output(
            ["xcrun", "--sdk", "macosx", "--show-sdk-path"],
            text=True,
        ).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


APPLE_CLANG_ENV = {
    "CC": "/usr/bin/clang",
    "CXX": "/usr/bin/clang++",
    "CMAKE_C_COMPILER": "/usr/bin/clang",
    "CMAKE_CXX_COMPILER": "/usr/bin/clang++",
}
# Why: Pin the toolchain explicitly to Apple Clang on macOS. This ensures we
# use the SDK-aware compiler that ships with Xcode/Command Line Tools, avoiding
# ABI and header-resolution pitfalls that can arise with third-party builds of
# LLVM/GCC. We also propagate the same compilers into CMake so native targets
# and ExternalProject dependencies build with a consistent toolchain.
_sdkroot = _detect_sdkroot()
if _sdkroot:
    APPLE_CLANG_ENV.setdefault("SDKROOT", _sdkroot)


def _detect_macos_arch() -> str | None:
    """Return the canonical macOS architecture string for CMake if applicable.

    Why: On Apple Silicon, many tools can run under Rosetta or native, and
    universal binaries may select a default slice unexpectedly. Explicitly
    telling CMake which arch to build (``arm64`` or ``x86_64``) prevents
    accidental cross-arch or mismatched builds.

    Strategy:
    - If not on macOS, do nothing.
    - If ``CMAKE_OSX_ARCHITECTURES`` is already set in the environment, respect
      it to allow overrides (e.g., cross-building).
    - Else infer from ``platform.machine()``.
    """
    if platform.system() != "Darwin":
        return None
    arch = os.environ.get("CMAKE_OSX_ARCHITECTURES")
    if arch:
        return arch
    machine = platform.machine().lower()
    if machine in {"arm64", "aarch64"}:
        return "arm64"
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    return None


_macos_arch = _detect_macos_arch()
if _macos_arch:
    APPLE_CLANG_ENV.setdefault("CMAKE_OSX_ARCHITECTURES", _macos_arch)


def _cmake_sysroot_flag() -> str:
    """Build and return the CMake flag to set ``CMAKE_OSX_SYSROOT`` if needed.

    The leading space is intentional so that callers can safely concatenate the
    return value into command fragments without additional spacing logic.
    """
    if _sdkroot:
        return f" -DCMAKE_OSX_SYSROOT={shlex.quote(_sdkroot)}"
    return ""


def _cmake_arch_flag() -> str:
    """Build and return the CMake flag to set ``CMAKE_OSX_ARCHITECTURES``.

    As above, a leading space is included to make concatenation ergonomic.
    """
    if _macos_arch:
        return f" -DCMAKE_OSX_ARCHITECTURES={shlex.quote(_macos_arch)}"
    return ""


def _compiler_env(**overrides: str) -> dict[str, str]:
    """Return a process environment dict with our compiler/SDK defaults merged.

    Callers can pass keyword-only ``overrides`` to tweak specific values for a
    single subprocess run while keeping the baseline deterministic settings.
    """
    env = os.environ.copy()
    env.update(APPLE_CLANG_ENV)
    env.update(overrides)
    return env


def _cmake_systemc_flags() -> str:
    """Return CMake flags that reuse a pre-installed SystemC, if provided.

    The Docker/devcontainer flow installs SystemC under /opt/systemc and sets
    TOYSSD_SYSTEMC_PREFIX so local configuration can skip FetchContent. When
    those env vars are not explicitly provided (e.g., running inside a Docker
    container that already has /opt/systemc), fall back to auto-detecting the
    prefix so we avoid rebuilding SystemC redundantly.
    """
    flags: list[str] = []
    prefix = os.environ.get("TOYSSD_SYSTEMC_PREFIX")
    if not prefix:
        candidate = Path("/opt/systemc")
        if (candidate / "lib").exists():
            prefix = str(candidate)
    fetch = os.environ.get("TOYSSD_FETCH_SYSTEMC")
    if prefix:
        flags.append(f"-DTOYSSD_SYSTEMC_PREFIX={shlex.quote(prefix)}")
        if fetch is None:
            fetch = "OFF"
    if fetch is not None:
        flags.append(f"-DTOYSSD_FETCH_SYSTEMC={shlex.quote(fetch)}")
    if not flags:
        return ""
    return " " + " ".join(flags)


def _python() -> str:
    """Resolve the Python interpreter path used for running Python tools/tests.

    Prefer ``$PYTHON`` if the user or CI provided one (e.g., a virtualenv
    interpreter). Otherwise prefer the local `.venv` created by ``uv sync`` to
    keep tooling aligned with the resolved dependencies, and fall back to the
    current interpreter for this process (``sys.executable``) or "python3".
    """
    if python := os.environ.get("PYTHON"):
        return python
    venv = ROOT / ".venv"
    if platform.system() == "Windows":
        candidate = venv / "Scripts" / "python.exe"
    else:
        candidate = venv / "bin" / "python3"
    if candidate.exists():
        return str(candidate)
    return sys.executable or "python3"


@task
def bootstrap(ctx) -> None:
    """Install Python deps and configure an initial Debug CMake build tree.

    Why Debug: a Debug tree speeds up edit/compile/test loops and enables
    sanitizers or asserts if later enabled. It also provides a populated build
    directory for code intelligence tools (compile_commands.json).

    Behavior:
    - If the "uv" package manager is available we use it for fast, reproducible
      Python dependency resolution (with the "dev" extra). Otherwise we fall
      back to pip editable install.
    - Configure CMake with tests enabled so CTest is ready.
    """
    uv = shutil.which("uv")
    if uv:
        ctx.run(f"{uv} sync --extra dev", pty=True, env=_compiler_env())
    else:
        ctx.run(
            f"{_python()} -m pip install --upgrade pip", pty=True, env=_compiler_env()
        )
        ctx.run(
            f"{_python()} -m pip install -e {ROOT / 'python'}[dev]",
            pty=True,
            env=_compiler_env(),
        )

    debug_dir = BUILD_DIR / "debug"
    debug_dir.mkdir(parents=True, exist_ok=True)
    # Configure a Debug build tree with explicit macOS sysroot/arch flags when
    # applicable. Enabling tests here ensures dependencies for unit tests are
    # also downloaded/configured (e.g., googletest via CMake's FetchContent).
    ctx.run(
        f"cmake -S {ROOT} -B {debug_dir} -DCMAKE_BUILD_TYPE=Debug"
        f"{_cmake_sysroot_flag()}{_cmake_arch_flag()}{_cmake_systemc_flags()} -DTOYSSD_BUILD_TESTS=ON",
        pty=True,
        env=_compiler_env(),
    )


@task(optional=["config"])
def build(ctx, config: str = "Debug") -> None:
    """Build the SystemC core, examples, and tests for a given configuration.

    The default configuration is "Debug" to optimize for iteration speed.
    Passing Release/RelWithDebInfo/MinSizeRel follows standard CMake semantics.
    We always re-run CMake to allow switching configurations safely.
    """
    build_dir = BUILD_DIR / config.lower()
    build_dir.mkdir(parents=True, exist_ok=True)
    # Always generate with explicit sysroot/arch flags on macOS to keep build
    # artifacts consistent across machines and CI.
    ctx.run(
        f"cmake -S {ROOT} -B {build_dir} -DCMAKE_BUILD_TYPE={config}"
        f"{_cmake_sysroot_flag()}{_cmake_arch_flag()}{_cmake_systemc_flags()} -DTOYSSD_BUILD_TESTS=ON",
        pty=True,
        env=_compiler_env(),
    )
    # Use parallel builds where supported to reduce build times.
    ctx.run(
        f"cmake --build {build_dir} --parallel",
        pty=True,
        env=_compiler_env(),
    )


@task(optional=["config"])
def test(ctx, config: str = "Debug") -> None:
    """Run both C++ (CTest/GTest) and Python (pytest) test suites.

    We build the C++ unit test target explicitly first for clearer error
    messages, then run ``ctest`` to execute all registered tests with full
    output on failure. Python tests are run after native tests to surface
    integration errors even if C++ tests pass.
    """
    build_dir = BUILD_DIR / config.lower()
    if not build_dir.exists():
        raise RuntimeError(
            f"Build directory {build_dir} does not exist. Run `invoke build --config {config}` first."
        )
    # Compile C++ tests explicitly to get fast compile errors before running CTest.
    ctx.run(
        f"cmake --build {build_dir} --target toyssd_core_tests",
        pty=True,
        env=_compiler_env(),
    )
    # Run all CTest tests and show verbose failure logs for quicker diagnosis.
    ctx.run(
        f"ctest --test-dir {build_dir} --output-on-failure",
        pty=True,
        env=_compiler_env(),
    )
    # Finally, run Python tests in the active interpreter/environment.
    ctx.run(f"{_python()} -m pytest", pty=True)


@task(optional=["fail_under_cpp", "fail_under_python"])
def coverage(
    ctx,
    fail_under_cpp: str | None = None,
    fail_under_python: str | None = None,
) -> None:
    """Build coverage-instrumented binaries and emit C++/Python reports.

    Reports:
    - C++: gcovr HTML + Cobertura-format XML under coverage/cpp/
    - Python: pytest-cov HTML + XML under coverage/python/

    Threshold arguments are optional and mainly used in CI (see docs/coverage_design.md).
    """

    build_dir = BUILD_DIR / "coverage"
    build_dir.mkdir(parents=True, exist_ok=True)

    cpp_dir = COVERAGE_DIR / "cpp"
    python_dir = COVERAGE_DIR / "python"
    python_html_dir = python_dir / "html"

    # Always start from a clean slate so HTML indexes refresh on every run.
    for stale in (cpp_dir, python_dir):
        if stale.exists():
            shutil.rmtree(stale)

    for path in (COVERAGE_DIR, cpp_dir, python_dir, python_html_dir):
        path.mkdir(parents=True, exist_ok=True)

    configure_cmd = (
        f"cmake -S {ROOT} -B {build_dir} -DCMAKE_BUILD_TYPE=Debug"
        f"{_cmake_sysroot_flag()}{_cmake_arch_flag()}{_cmake_systemc_flags()}"
        " -DTOYSSD_BUILD_TESTS=ON -DTOYSSD_ENABLE_COVERAGE=ON"
    )
    ctx.run(configure_cmd, pty=True, env=_compiler_env())

    ctx.run(
        f"cmake --build {build_dir} --parallel",
        pty=True,
        env=_compiler_env(),
    )
    ctx.run(
        f"ctest --test-dir {build_dir} --output-on-failure",
        pty=True,
        env=_compiler_env(),
    )

    cpp_index = cpp_dir / "index.html"
    cpp_xml = cpp_dir / "cobertura.xml"
    gcovr_base = (
        f"gcovr -r {shlex.quote(str(ROOT))} --object-directory {shlex.quote(str(build_dir))}"
        " --branches"
    )
    exclude_patterns = [
        r".*/tests/.*",
        r".*/examples/.*",
        r".*/third_party/.*",
        r".*/opt/systemc.*",
        r".*/_deps/.*",
    ]
    for pattern in exclude_patterns:
        gcovr_base += f" --exclude {shlex.quote(pattern)}"
    llvm_cov = shutil.which("llvm-cov")
    if llvm_cov:
        gcovr_base += f" --gcov-executable {shlex.quote(f'{llvm_cov} gcov')}"
    if fail_under_cpp:
        try:
            threshold = int(fail_under_cpp)
        except ValueError as exc:  # pragma: no cover - defensive against Invoke args
            raise RuntimeError("fail_under_cpp must be an integer") from exc
        gcovr_html_cmd = gcovr_base + f" --fail-under-line={threshold}"
    else:
        gcovr_html_cmd = gcovr_base

    gcovr_html_cmd += f" --html-details --output {shlex.quote(str(cpp_index))}"
    gcovr_failed = False
    result = ctx.run(gcovr_html_cmd, pty=True, warn=True)
    if result.exited != 0:
        gcovr_failed = True

    gcovr_xml_cmd = gcovr_base + f" --xml-pretty --output {shlex.quote(str(cpp_xml))}"
    result = ctx.run(gcovr_xml_cmd, pty=True, warn=True)
    if result.exited != 0:
        gcovr_failed = True

    python_xml = python_dir / "coverage.xml"
    pytest_cmd = (
        f"{_python()} -m pytest -q"
        f" --cov=toyssd --cov-branch"
        f" --cov-report=xml:{python_xml}"
        f" --cov-report=html:{python_html_dir}"
        " --cov-report=term"
    )
    if fail_under_python:
        try:
            py_threshold = int(fail_under_python)
        except ValueError as exc:  # pragma: no cover
            raise RuntimeError("fail_under_python must be an integer") from exc
        pytest_cmd += f" --cov-fail-under={py_threshold}"
    pytest_failed = False
    result = ctx.run(pytest_cmd, pty=True, warn=True)
    if result.exited != 0:
        pytest_failed = True

    if gcovr_failed or pytest_failed:
        reasons = []
        if gcovr_failed:
            reasons.append("C++ coverage thresholds")
        if pytest_failed:
            reasons.append("Python coverage thresholds")
        raise Exit(f"Coverage task failed ({', '.join(reasons)}). See coverage artifacts for details.", code=1)


@task
def format(ctx) -> None:
    """Apply clang-format to C++ and ruff format to Python sources.

    Why both: keep a consistent code style across languages and ensure diffs
    remain focused on semantic changes. ``ruff format`` is fast and compatible
    with Black-style formatting; clang-format is the de-facto standard for C++.
    """
    sources = " ".join(str(path) for path in _iter_sources())
    if sources:
        ctx.run(f"clang-format -i {sources}", pty=True)
    ctx.run(f"{_python()} -m ruff format python", pty=True)


@task
def lint_markdown(ctx) -> None:
    """Run markdown linter on all documentation files.

    Uses pymarkdownlnt to check markdown files for style issues, broken links,
    and structural problems. This helps maintain consistent, readable
    documentation across the project.
    """
    md_files = " ".join(shlex.quote(str(path)) for path in _iter_markdown_files())
    config_path = shlex.quote(str(ROOT / ".pymarkdown.json"))
    if md_files:
        ctx.run(f"{_python()} -m pymarkdown -c {config_path} scan {md_files}", pty=True)


@task
def lint(ctx) -> None:
    """Run C++, Python, and Markdown linters: clang-tidy, cpplint, ruff, and pymarkdown.

    Why both static analyzers (clang-tidy) and style linters (cpplint/ruff):
    they catch different classes of issues. We configure a Debug build first to
    ensure compile commands exist for clang-tidy and that generated headers (if
    any) are available.
    """
    build_dir = BUILD_DIR / "debug"
    build_dir.mkdir(parents=True, exist_ok=True)
    ctx.run(
        f"cmake -S {ROOT} -B {build_dir} -DCMAKE_BUILD_TYPE=Debug"
        f"{_cmake_sysroot_flag()}{_cmake_arch_flag()}{_cmake_systemc_flags()} -DTOYSSD_BUILD_TESTS=ON",
        pty=True,
        env=_compiler_env(),
    )
    # Build the primary library target to generate accurate compile commands and
    # ensure headers are discoverable for analysis.
    ctx.run(
        f"cmake --build {build_dir} --target toyssd_core -- -k",
        pty=True,
        env=_compiler_env(),
    )
    cpp_files = " ".join(shlex.quote(path) for path in _iter_cpp_sources())
    config_path = shlex.quote(str(ROOT / ".clang-tidy"))
    build_arg = shlex.quote(str(build_dir))
    header_filter = shlex.quote(r"^(src|include)/")
    ctx.run(
        f"clang-tidy -p {build_arg} --config-file={config_path} --header-filter={header_filter} {cpp_files}",
        pty=True,
        env=_compiler_env(),
    )
    ctx.run("cpplint --recursive include src", pty=True)
    ctx.run(f"{_python()} -m ruff check python", pty=True)
    # Run markdown linting on all documentation files.
    lint_markdown(ctx)


@task
def clean(ctx) -> None:
    """Remove all build artifacts and generated files.

    This task deletes the entire build/ directory, allowing developers to start
    from a clean state. It's safe to run multiple times (idempotent) and useful
    when switching between configurations or troubleshooting build problems.

    After running clean, you'll need to run bootstrap (or build) again to
    reconfigure the CMake build tree.
    """
    if BUILD_DIR.exists():
        print(f"Removing {BUILD_DIR}...")
        shutil.rmtree(BUILD_DIR)
        print("Build directory cleaned successfully.")
    else:
        print(f"Build directory {BUILD_DIR} does not exist. Nothing to clean.")


def _iter_sources() -> list[Path]:
    """Return all format-able C++ source files (headers and implementation).

    We scan only the checked-in ``include`` and ``src`` trees to avoid
    formatting generated code under ``build/``.
    """
    return [*Path("include").rglob("*.hpp"), *Path("src").rglob("*.cpp")]


def _iter_cpp_sources() -> list[str]:
    """Return C++ implementation files used by clang-tidy and cpplint runs.

    Returned as strings for direct CLI argument concatenation.
    """
    return [str(path) for path in Path("src").rglob("*.cpp")]


def _iter_markdown_files() -> list[Path]:
    """Return all markdown documentation files for linting.

    Scans the root directory and docs/ subdirectory for .md files to maintain
    consistent documentation style.
    """
    md_files = []
    # Root-level markdown files
    for path in ROOT.glob("*.md"):
        md_files.append(path)
    # Documentation directory markdown files
    docs_dir = ROOT / "docs"
    if docs_dir.exists():
        md_files.extend(docs_dir.rglob("*.md"))
    return md_files


ns = Collection()
# Expose tasks under the default namespace so developers can run e.g.:
#   invoke bootstrap
#   invoke build --config Release
#   invoke test
ns.add_task(bootstrap)
ns.add_task(build)
ns.add_task(test)
ns.add_task(coverage)
ns.add_task(format)
ns.add_task(lint)
ns.add_task(lint_markdown)
ns.add_task(clean)
