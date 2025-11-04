"""Invoke task collection for toyssd development workflow."""

from __future__ import annotations

import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from invoke import Collection, task

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"


def _detect_sdkroot() -> str | None:
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


APPLE_CLANG_ENV = {"CC": "/usr/bin/clang", "CXX": "/usr/bin/clang++"}
_sdkroot = _detect_sdkroot()
if _sdkroot:
    APPLE_CLANG_ENV.setdefault("SDKROOT", _sdkroot)


def _compiler_env(**overrides: str) -> dict[str, str]:
    env = os.environ.copy()
    env.update(APPLE_CLANG_ENV)
    env.update(overrides)
    return env


def _python() -> str:
    return os.environ.get("PYTHON", sys.executable or "python3")


@task
def bootstrap(ctx) -> None:
    """Install Python dependencies and configure CMake build trees."""
    uv = shutil.which("uv")
    if uv:
        ctx.run(f"{uv} sync --extra dev", pty=True, env=_compiler_env())
    else:
        ctx.run(f"{_python()} -m pip install --upgrade pip", pty=True, env=_compiler_env())
        ctx.run(
            f"{_python()} -m pip install -e {ROOT / 'python'}[dev]",
            pty=True,
            env=_compiler_env(),
        )

    debug_dir = BUILD_DIR / "debug"
    debug_dir.mkdir(parents=True, exist_ok=True)
    ctx.run(
        f"cmake -S {ROOT} -B {debug_dir} -DCMAKE_BUILD_TYPE=Debug",
        pty=True,
        env=_compiler_env(),
    )


@task(optional=["config"])
def build(ctx, config: str = "Debug") -> None:
    """Build the SystemC core and examples."""
    build_dir = BUILD_DIR / config.lower()
    build_dir.mkdir(parents=True, exist_ok=True)
    ctx.run(
        f"cmake -S {ROOT} -B {build_dir} -DCMAKE_BUILD_TYPE={config}",
        pty=True,
        env=_compiler_env(),
    )
    ctx.run(
        f"cmake --build {build_dir} --parallel",
        pty=True,
        env=_compiler_env(),
    )


@task(optional=["config"])
def test(ctx, config: str = "Debug") -> None:
    """Run C++ and Python test suites."""
    build_dir = BUILD_DIR / config.lower()
    if not build_dir.exists():
        raise RuntimeError(
            f"Build directory {build_dir} does not exist. Run `invoke build --config {config}` first."
        )
    ctx.run(
        f"cmake --build {build_dir} --target toyssd_core_tests",
        pty=True,
        env=_compiler_env(),
    )
    ctx.run(
        f"ctest --test-dir {build_dir} --output-on-failure",
        pty=True,
        env=_compiler_env(),
    )
    ctx.run(f"{_python()} -m pytest", pty=True)


@task
def format(ctx) -> None:
    """Apply clang-format to C++ sources and ruff to Python sources."""
    sources = " ".join(str(path) for path in _iter_sources())
    if sources:
        ctx.run(f"clang-format -i {sources}", pty=True)
    ctx.run(f"{_python()} -m ruff format python", pty=True)


@task
def lint(ctx) -> None:
    """Run clang-tidy, cpplint, and ruff on the codebase."""
    build_dir = BUILD_DIR / "debug"
    build_dir.mkdir(parents=True, exist_ok=True)
    ctx.run(
        f"cmake -S {ROOT} -B {build_dir} -DCMAKE_BUILD_TYPE=Debug",
        pty=True,
        env=_compiler_env(),
    )
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


def _iter_sources() -> list[Path]:
    return [*Path("include").rglob("*.hpp"), *Path("src").rglob("*.cpp")]


def _iter_cpp_sources() -> list[str]:
    return [str(path) for path in Path("src").rglob("*.cpp")]


ns = Collection()
ns.add_task(bootstrap)
ns.add_task(build)
ns.add_task(test)
ns.add_task(format)
ns.add_task(lint)
