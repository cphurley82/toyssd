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


class ToolChecker:
    """Handles toolchain verification with color-coded output."""

    # ANSI color codes
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"
    CYAN = "\033[36m"
    RESET = "\033[0m"

    def __init__(self, context):
        self.context = context
        self.missing_required = []
        self.missing_optional = []

    def ok(self, msg: str) -> None:
        print(f"{self.GREEN}✔ {msg}{self.RESET}")

    def warn(self, msg: str) -> None:
        print(f"{self.YELLOW}⚠ {msg}{self.RESET}")

    def fail(self, msg: str) -> None:
        print(f"{self.RED}✖ {msg}{self.RESET}")

    def run_silent(self, cmd: str) -> tuple[bool, str]:
        r = self.context.run(cmd, warn=True, hide=True)
        return r.ok, r.stdout.strip()

    def check_cmake(self) -> None:
        """Check for required cmake installation."""
        cmake_ok, cmake_ver = self.run_silent("cmake --version")
        if cmake_ok:
            first = cmake_ver.splitlines()[0] if cmake_ver else "cmake (version unknown)"
            self.ok(first)
        else:
            self.fail("cmake not found (required). Install cmake and ensure it's on PATH.")
            self.missing_required.append("cmake")

    def check_uv(self) -> None:
        """Check for required uv installation."""
        uv_ok, uv_ver = self.run_silent("uv --version")
        if uv_ok:
            self.ok(f"uv {uv_ver}")
        else:
            self.fail("uv not found (required). Install via Homebrew or follow uv docs.")
            self.missing_required.append("uv")

    def check_clang_format(self) -> None:
        """Check for optional clang-format installation."""
        cf_ok, cf_ver = self.run_silent("clang-format --version")
        if cf_ok:
            self.ok(cf_ver.splitlines()[0])
        else:
            self.warn("clang-format not found (optional). CMake format targets will no-op.")
            self.missing_optional.append("clang-format")

    def check_clang_tidy(self) -> None:
        """Check for optional clang-tidy installation."""
        ct_ok, ct_ver = self.run_silent("clang-tidy --version")
        if ct_ok:
            self.ok(ct_ver.splitlines()[0])
        else:
            self.warn(
                "clang-tidy not found (optional). Enable via Homebrew or your toolchain; "
                "Invoke cpp_analyze will skip if unavailable."
            )
            self.missing_optional.append("clang-tidy")

    def check_cpplint(self) -> None:
        """Check for optional cpplint installation."""
        cpplint_present, _ = self.run_silent(
            "uv run python -c 'import importlib.util as util,sys; "
            'sys.exit(0 if util.find_spec("cpplint") else 1)\''
        )
        if cpplint_present:
            self._check_cpplint_venv()
        else:
            self._check_cpplint_system()

    def _check_cpplint_venv(self) -> None:
        """Check cpplint version in venv."""
        meta_ok, meta_out = self.run_silent(
            "uv run python -c 'import importlib.metadata as m; print(m.version(\"cpplint\"))'"
        )
        if meta_ok and meta_out:
            self.ok(f"cpplint {meta_out.strip()} (uv venv)")
        else:
            ver = self._get_cpplint_fallback_version()
            self.ok(f"cpplint {ver} (uv venv)")

    def _get_cpplint_fallback_version(self) -> str:
        """Get cpplint version using fallback methods."""
        mod_ok, mod_out = self.run_silent(
            "uv run python -c 'import cpplint; "
            'print(getattr(cpplint,"__version__","unknown"))\''
        )
        ver = mod_out.strip() if mod_ok and mod_out else "unknown"
        if ver == "unknown":
            cli_ok, cli_out = self.run_silent("uv run cpplint --version")
            if cli_ok and cli_out:
                ver_line = next(
                    (ln for ln in cli_out.splitlines() if ln.lower().startswith("cpplint ")),
                    None,
                )
                if ver_line:
                    ver = ver_line.strip().split(" ", 1)[-1]
        return ver

    def _check_cpplint_system(self) -> None:
        """Check for cpplint on system PATH."""
        cli_ok, cli_out = self.run_silent("cpplint --version")
        if cli_ok and cli_out:
            ver_line = next(
                (ln for ln in cli_out.splitlines() if ln.lower().startswith("cpplint ")), None
            )
            if ver_line:
                self.ok(f"cpplint {ver_line.strip().split(' ', 1)[-1]} (system CLI)")
            else:
                self.ok("cpplint (system CLI) present")
            self.warn(
                "cpplint not installed in uv venv — CTest cpplint may be skipped. "
                "Run: uv sync --all-extras"
            )
        else:
            self.warn("cpplint not found (optional). Run: uv sync --all-extras")
        self.missing_optional.append("cpplint")

    def print_summary(self) -> None:
        """Print final environment check summary."""
        if self.missing_required:
            self.fail(
                f"Environment check: FAIL — missing required tools: "
                f"{', '.join(self.missing_required)}"
            )
        elif self.missing_optional:
            self.warn(
                "Environment check: PASS (required OK) — missing optional: "
                f"{', '.join(self.missing_optional)}"
            )
        else:
            self.ok("Environment check: PASS — all tools present")


@task
def env_check(c):
    """Verify required and optional tools with color-coded summary."""
    checker = ToolChecker(c)
    print(f"{checker.CYAN}Checking toolchain...{checker.RESET}")

    checker.check_cmake()
    checker.check_uv()
    checker.check_clang_format()
    checker.check_clang_tidy()
    checker.check_cpplint()

    checker.print_summary()


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
