from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass


@dataclass
class RepoPaths:
    root: str

    @property
    def default_build(self) -> str:
        # Prefer build-debug if it exists; otherwise fall back to build
        cand = os.path.join(self.root, "build-debug")
        if os.path.isdir(cand):
            return cand
        return os.path.join(self.root, "build")


def find_repo_root() -> str:
    here = os.path.abspath(os.path.dirname(__file__))
    # python/toyssd -> python -> repo root
    return os.path.abspath(os.path.join(here, os.pardir, os.pardir))


# ----------------------- subcommands -----------------------


def cmd_gen_config(args: argparse.Namespace) -> int:
    cfg = {
        "nand": {
            "dies": args.dies,
            "blocks_per_die": args.blocks,
            "pages_per_block": args.ppb,
            "page_size_bytes": args.psize,
            "timing": {"t_read_us": 50, "t_prog_us": 600, "t_erase_us": 3000},
        },
        "controller": {"ctrl_overhead_us": 5},
        "rng_seed": 42,
    }
    out_path = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
    print(f"[OK] Wrote {out_path}")
    return 0


def cmd_analyze_results(args: argparse.Namespace) -> int:
    try:
        import matplotlib.pyplot as plt
        import pandas as pd
    except Exception as e:  # pragma: no cover - optional dependency
        print(
            "This command requires the 'viz' extras (pandas, matplotlib).\n"
            "Install with: uv sync --extra viz or uv pip install pandas matplotlib",
            file=sys.stderr,
        )
        print(f"Import error: {e}", file=sys.stderr)
        return 2

    df = pd.read_csv(args.input)
    print(df.describe())
    if "latency_us" in df.columns:
        df["latency_us"].hist(bins=50)
        plt.xlabel("Latency (us)")
        plt.ylabel("Count")
        plt.title("Latency Histogram")
        plt.show()
    return 0


def cmd_plot_metrics(args: argparse.Namespace) -> int:
    try:
        import matplotlib.pyplot as plt
        import pandas as pd
    except Exception as e:  # pragma: no cover - optional dependency
        print(
            "This command requires the 'viz' extras (pandas, matplotlib).\n"
            "Install with: uv sync --extra viz or uv pip install pandas matplotlib",
            file=sys.stderr,
        )
        print(f"Import error: {e}", file=sys.stderr)
        return 2

    df = pd.read_csv(args.csv)
    for col in ["latency_us"]:
        if col in df.columns:
            df[col].plot(kind="hist", bins=50, title=col)
            plt.show()
    return 0


def cmd_run_fio_demo(args: argparse.Namespace) -> int:
    """Run the bundled fio demo via CTest in the chosen build directory.

    This mirrors the CTest target registered by CMake (label: demo), avoiding
    the need to rebuild engine paths/env vars here.
    """
    repo = RepoPaths(find_repo_root())
    bdir = os.path.abspath(args.build_dir or repo.default_build)
    if not os.path.isdir(bdir):
        print(f"Build dir not found: {bdir}. Configure and build first.", file=sys.stderr)
        return 2
    cmd: list[str] = [
        "ctest",
        "--test-dir",
        bdir,
        "--output-on-failure",
        "-L",
        "demo",
    ]
    print("[INFO] Running:", " ".join(cmd))
    return subprocess.call(cmd)


# ----------------------- CLI wiring -----------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="toyssd", description="Toy SSD Simulator helper CLI")
    sub = p.add_subparsers(dest="command", required=True)

    s = sub.add_parser("gen-config", help="Generate a JSON config file")
    s.add_argument("--dies", type=int, default=2)
    s.add_argument("--blocks", type=int, default=256)
    s.add_argument("--ppb", type=int, default=128, help="Pages per block")
    s.add_argument("--psize", type=int, default=4096, help="Page size in bytes")
    s.add_argument("--out", default="config/generated.json")
    s.set_defaults(func=cmd_gen_config)

    s = sub.add_parser("analyze-results", help="Analyze CSV results and show quick stats")
    s.add_argument("--input", required=True, help="Path to CSV file")
    s.set_defaults(func=cmd_analyze_results)

    s = sub.add_parser("plot-metrics", help="Plot simple metrics from a CSV file")
    s.add_argument("--csv", required=True)
    s.set_defaults(func=cmd_plot_metrics)

    s = sub.add_parser("run-fio-demo", help="Run the bundled fio demo via CTest")
    s.add_argument(
        "--build-dir", default=None, help="Build directory (default: build-debug or build)"
    )
    s.set_defaults(func=cmd_run_fio_demo)

    return p


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    ns = parser.parse_args(argv)
    return ns.func(ns)


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
