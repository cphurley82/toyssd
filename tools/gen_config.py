import argparse
import json
from pathlib import Path


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a simulator config JSON.")
    parser.add_argument("--dies", type=int, default=2, help="Number of NAND dies.")
    parser.add_argument(
        "--blocks",
        type=int,
        default=256,
        help="Blocks per die.",
    )
    parser.add_argument(
        "--ppb",
        type=int,
        default=128,
        help="Pages per block.",
    )
    parser.add_argument(
        "--psize",
        type=int,
        default=4096,
        help="Page size in bytes.",
    )
    parser.add_argument(
        "--out",
        default="config/generated.json",
        help="Output JSON path.",
    )
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    cfg = {
        "nand": {
            "dies": args.dies,
            "blocks_per_die": args.blocks,
            "pages_per_block": args.ppb,
            "page_size_bytes": args.psize,
            "timing": {
                "t_read_us": 50,
                "t_prog_us": 600,
                "t_erase_us": 3000,
            },
        },
        "controller": {"ctrl_overhead_us": 5},
        "rng_seed": 42,
    }
    output_path = Path(args.out)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as handle:
        json.dump(cfg, handle, indent=2)
        handle.write("\n")
    print("[OK] Wrote", output_path)


if __name__ == "__main__":
    main()
