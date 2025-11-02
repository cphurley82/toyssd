import argparse
import subprocess
import sys
from pathlib import Path


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run fio against the toyssd engine.")
    parser.add_argument(
        "--config",
        default="config/default.json",
        help="Path to simulator JSON configuration.",
    )
    parser.add_argument(
        "--runtime",
        type=int,
        default=5,
        help="Runtime for the fio workload (seconds).",
    )
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    build_dir = repo_root / "build"
    fio = build_dir / "_deps" / "fio-src" / "fio"
    ioengine_name = "libssdsim.dylib" if sys.platform == "darwin" else "libssdsim.so"
    ioengine = build_dir / ioengine_name
    config = (repo_root / args.config).resolve()

    cmd = [
        str(fio),
        f"--ioengine=external:{ioengine}",
        f"--filename={config}",
        "--name=demo",
        "--rw=randwrite",
        "--size=64M",
        "--bs=4k",
        "--iodepth=8",
        "--numjobs=1",
        "--time_based",
        f"--runtime={args.runtime}",
    ]

    print("[INFO] Running:", " ".join(cmd))
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
