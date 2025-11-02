from __future__ import annotations

import os

from invoke import task

# ----------------------- helpers -----------------------


def build_dir_for(backend: str, build_type: str) -> str:
    if backend == "docker":
        return "build-docker-debug" if build_type == "Debug" else "build-docker-release"
    return "build-debug" if build_type == "Debug" else "build-release"


def get_backend(c, override: str | None = None) -> str:
    if override:
        return override
    cfg = getattr(c, "config", None)
    if cfg and hasattr(cfg, "toyssd") and isinstance(cfg.toyssd, dict) and "backend" in cfg.toyssd:
        return cfg.toyssd["backend"]
    return os.environ.get("TOYSSD_BACKEND", "native")


def run_cmd(c, cmd: str, backend: str, image: str = "toyssd"):
    if backend == "native":
        return c.run(cmd, pty=True)
    # docker backend
    repo = c.run("pwd", hide=True).stdout.strip()
    uid = c.run("id -u", hide=True).stdout.strip()
    gid = c.run("id -g", hide=True).stdout.strip()
    docker_cmd = (
        f'docker run --rm -t --user "{uid}:{gid}" -e HOME=/src '
        f'-v "{repo}":/src -w /src {image} {cmd}'
    )
    return c.run(docker_cmd, pty=True)


# ----------------------- env/tasks -----------------------


@task
def env_check(c):
    """Verify required and optional tools with color-coded summary."""
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"
    CYAN = "\033[36m"
    RESET = "\033[0m"

    def ok(msg: str):
        print(f"{GREEN}✔ {msg}{RESET}")

    def warn(msg: str):
        print(f"{YELLOW}⚠ {msg}{RESET}")

    def fail(msg: str):
        print(f"{RED}✖ {msg}{RESET}")

    def run_silent(cmd: str):
        r = c.run(cmd, warn=True, hide=True)
        return r.ok, r.stdout.strip()

    print(f"{CYAN}Checking toolchain...{RESET}")

    missing_required = []
    missing_optional = []

    # Required: cmake
    cmake_ok, cmake_ver = run_silent("cmake --version")
    if cmake_ok:
        first = cmake_ver.splitlines()[0] if cmake_ver else "cmake (version unknown)"
        ok(first)
    else:
        fail("cmake not found (required). Install cmake and ensure it's on PATH.")
        missing_required.append("cmake")

    # Required: uv
    uv_ok, uv_ver = run_silent("uv --version")
    if uv_ok:
        ok(f"uv {uv_ver}")
    else:
        fail("uv not found (required). Install via Homebrew or follow uv docs.")
        missing_required.append("uv")

    # Optional: clang-format
    cf_ok, cf_ver = run_silent("clang-format --version")
    if cf_ok:
        ok(cf_ver.splitlines()[0])
    else:
        warn("clang-format not found (optional). CMake format targets will no-op.")
        missing_optional.append("clang-format")

    # Optional: clang-tidy
    ct_ok, ct_ver = run_silent("clang-tidy --version")
    if ct_ok:
        ok(ct_ver.splitlines()[0])
    else:
        warn(
            "clang-tidy not found (optional). Enable via Homebrew or your toolchain; "
            "Invoke cpp_analyze will skip if unavailable."
        )
        missing_optional.append("clang-tidy")

    # Optional: cpplint — prefer venv module (for CTest), then CLI as fallback for version reporting
    cpplint_present, _ = run_silent(
        "uv run python -c 'import importlib.util as util,sys; "
        'sys.exit(0 if util.find_spec("cpplint") else 1)\''
    )
    if cpplint_present:
        # Try importlib.metadata for the distribution version; fallback to module __version__
        # as last resort, CLI
        meta_ok, meta_out = run_silent(
            "uv run python -c 'import importlib.metadata as m; print(m.version(\"cpplint\"))'"
        )
        if meta_ok and meta_out:
            ok(f"cpplint {meta_out.strip()} (uv venv)")
        else:
            # Fallback to module attribute
            mod_ok, mod_out = run_silent(
                "uv run python -c 'import cpplint; "
                'print(getattr(cpplint,"__version__","unknown"))\''
            )
            ver = mod_out.strip() if mod_ok and mod_out else "unknown"
            if ver == "unknown":
                # Fallback to CLI in venv to extract version line if possible
                cli_ok, cli_out = run_silent("uv run cpplint --version")
                if cli_ok and cli_out:
                    ver_line = next(
                        (ln for ln in cli_out.splitlines() if ln.lower().startswith("cpplint ")),
                        None,
                    )
                    if ver_line:
                        ver = ver_line.strip().split(" ", 1)[-1]
            ok(f"cpplint {ver} (uv venv)")
    else:
        # Not importable in venv — see if a system CLI exists
        cli_ok, cli_out = run_silent("cpplint --version")
        if cli_ok and cli_out:
            ver_line = next(
                (ln for ln in cli_out.splitlines() if ln.lower().startswith("cpplint ")), None
            )
            if ver_line:
                ok(f"cpplint {ver_line.strip().split(' ', 1)[-1]} (system CLI)")
            else:
                ok("cpplint (system CLI) present")
            warn(
                "cpplint not installed in uv venv — CTest cpplint may be skipped. "
                "Run: uv sync --all-extras"
            )
        else:
            warn("cpplint not found (optional). Run: uv sync --all-extras")
        missing_optional.append("cpplint")

    # Summary
    if missing_required:
        fail(f"Environment check: FAIL — missing required tools: {', '.join(missing_required)}")
    elif missing_optional:
        warn(
            "Environment check: PASS (required OK) — missing optional: "
            f"{', '.join(missing_optional)}"
        )
    else:
        ok("Environment check: PASS — all tools present")


@task
def cpp_configure(c, build_type="Debug", backend=None, *, werror=True, tidy=False):
    backend = get_backend(c, backend)
    bdir = build_dir_for(backend, build_type)
    flags = [
        f"-DTOYSSD_ENABLE_WERROR={'ON' if werror else 'OFF'}",
    ]
    if tidy:
        flags.append("-DTOYSSD_ENABLE_CLANG_TIDY=ON")
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


@task
def cpp_format(c, build_type="Debug", backend=None):
    backend = get_backend(c, backend)
    bdir = build_dir_for(backend, build_type)
    # format target exists after first configure
    run_cmd(c, f"cmake --build {bdir} --target format", backend)


@task
def cpp_format_check(c, build_type="Debug", backend=None):
    backend = get_backend(c, backend)
    bdir = build_dir_for(backend, build_type)
    run_cmd(c, f"cmake --build {bdir} --target format-check", backend)


@task
def cpp_lint(c):
    # Delegate to CTest's cpplint test added by CMake at configure time
    # If the test isn't present (cpplint not found), ignore 'no tests' status.
    backend = get_backend(c, None)
    bdir = build_dir_for(backend, "Debug")
    run_cmd(c, f"ctest --test-dir {bdir} -R cpplint --output-on-failure --no-tests=ignore", backend)


@task
def cpp_analyze(c, build_type="Debug", backend=None):
    """Configure with per-target clang-tidy and force rebuild to run analysis."""
    backend = get_backend(c, backend)
    # Configure with tidy enabled
    cpp_configure(c, build_type=build_type, backend=backend, werror=True, tidy=True)
    bdir = build_dir_for(backend, build_type)
    # Force rebuild so clang-tidy executes on all targets
    run_cmd(c, f"cmake --build {bdir} --clean-first -j", backend)


@task
def cpp_coverage(c, backend=None, threshold: int | None = None):
    """Generate C++ coverage reports via gcovr."""
    backend = get_backend(c, backend)
    bdir = "build-coverage"
    threshold_flag = f"-DCODE_COVERAGE_THRESHOLD={threshold}" if threshold is not None else ""
    configure_cmd = f"cmake -S . -B {bdir} -DCMAKE_BUILD_TYPE=Debug -DENABLE_CODE_COVERAGE=ON"
    if threshold_flag:
        configure_cmd += f" {threshold_flag}"
    run_cmd(c, configure_cmd, backend)
    run_cmd(c, f"cmake --build {bdir} -j", backend)
    run_cmd(c, f"cmake --build {bdir} --target coverage", backend)


@task
def py_format(c):
    # Discover Python files (tracked or untracked)
    list_cmd = "find python tests -type f -name '*.py' 2>/dev/null"
    # Print the file list
    c.run("echo 'Ruff will format the following files:'", pty=True)
    c.run(list_cmd, pty=True)
    # Format using xargs to handle many files
    c.run(f"{list_cmd} | xargs -r -n 64 uv run ruff format", pty=True)


@task
def py_lint(c):
    # Discover Python files (tracked or untracked)
    list_cmd = "find python tests -type f -name '*.py' 2>/dev/null"
    # Print the file list
    c.run("echo 'Ruff will lint the following files:'", pty=True)
    c.run(list_cmd, pty=True)
    # Lint using xargs to handle many files
    c.run(f"{list_cmd} | xargs -r -n 64 uv run ruff check", pty=True)


@task
def py_typecheck(c):
    # Print files that mypy will type-check (approximate to all Python files)
    list_cmd = "find python tests -type f -name '*.py' 2>/dev/null"
    c.run("echo 'mypy will type-check the following files:'", pty=True)
    c.run(list_cmd, pty=True)
    c.run("uv run mypy", pty=True)


@task
def py_test(c, backend=None):
    backend = get_backend(c, backend)
    run_cmd(c, "uv run pytest", backend)


@task(pre=[cpp_format_check, cpp_lint, py_lint, py_typecheck])
def check(c):
    """Run static checks (no build/tests)."""
    pass


@task
def verify(c, build_type: str = "Debug", backend: str | None = None):
    """Full validation: configure first, then static checks, then build + tests.

    Rationale: C++ formatting and cpplint checks rely on CMake targets/tests
    created at configure time. On fresh CI runners, configure must run first.
    """
    # Ensure CMake targets and tests exist before running checks
    cpp_configure(c, build_type=build_type, backend=backend)
    # Static checks (CTests + Ruff/mypy)
    check(c)
    # Build and test
    cpp_build(c, build_type=build_type, backend=backend)
    cpp_test(c, build_type=build_type, backend=backend)
    py_test(c, backend=backend)


@task
def coverage(c, backend: str | None = None, threshold: int | None = None):
    """Run the coverage pipeline (configure, build, gcovr report)."""
    cpp_coverage(c, backend=backend, threshold=threshold)


@task
def demo(c, backend: str | None = None):
    """Run the fio demo CTest label."""
    backend = get_backend(c, backend)
    bdir = build_dir_for(backend, "Debug")
    run_cmd(c, f"ctest --test-dir {bdir} -L demo --output-on-failure", backend)


@task
def docker_cpp_configure(c, build_type="Debug", *, werror=True):
    cpp_configure(c, build_type=build_type, backend="docker", werror=werror)


@task
def docker_cpp_build(c, build_type="Debug"):
    cpp_build(c, build_type=build_type, backend="docker")


@task
def docker_cpp_test(c, build_type="Debug"):
    cpp_test(c, build_type=build_type, backend="docker")


@task
def docker_cpp_analyze(c, build_type="Debug"):
    cpp_analyze(c, build_type=build_type, backend="docker")
