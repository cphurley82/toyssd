"""Run fio benchmark against the toyssd simulator engine."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


# Default fio workload parameters
DEFAULT_RUNTIME_SECONDS = 5
DEFAULT_CONFIG_PATH = "config/default.json"
DEFAULT_WORKLOAD_SIZE = "64M"
DEFAULT_BLOCK_SIZE = "4k"
DEFAULT_IO_DEPTH = 8


def _parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description="Run fio against the toyssd engine.")
    parser.add_argument(
        "--config",
        default=DEFAULT_CONFIG_PATH,
        help="Path to simulator JSON configuration.",
    )
    parser.add_argument(
        "--runtime",
        type=int,
        default=DEFAULT_RUNTIME_SECONDS,
        help="Runtime for the fio workload (seconds).",
    )
    return parser.parse_args()


def get_ioengine_path(build_dir: Path) -> Path:
    """
    Get platform-specific ioengine library path.

    Args:
        build_dir: Path to build directory

    Returns:
        Path to ioengine shared library
    """
    ioengine_name = "libssdsim.dylib" if sys.platform == "darwin" else "libssdsim.so"
    return build_dir / ioengine_name


def build_fio_command(
    fio_path: Path, ioengine_path: Path, config_path: Path, runtime_seconds: int
) -> list[str]:
    """
    Build fio command line arguments.

    Args:
        fio_path: Path to fio executable
        ioengine_path: Path to ioengine library
        config_path: Path to simulator config
        runtime_seconds: Workload runtime in seconds

    Returns:
        List of command line arguments
    """
    return [
        str(fio_path),
        f"--ioengine=external:{ioengine_path}",
        f"--filename={config_path}",
        "--name=demo",
        "--rw=randwrite",
        f"--size={DEFAULT_WORKLOAD_SIZE}",
        f"--bs={DEFAULT_BLOCK_SIZE}",
        f"--iodepth={DEFAULT_IO_DEPTH}",
        "--numjobs=1",
        "--time_based",
        f"--runtime={runtime_seconds}",
    ]


def main() -> int:
    """Main entry point."""
    args = _parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    build_dir = repo_root / "build"
    fio_path = build_dir / "_deps" / "fio-src" / "fio"
    ioengine_path = get_ioengine_path(build_dir)
    config_path = (repo_root / args.config).resolve()

    cmd = build_fio_command(fio_path, ioengine_path, config_path, args.runtime)

    print("[INFO] Running:", " ".join(cmd))
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
