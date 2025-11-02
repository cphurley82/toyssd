"""Generate simulator configuration JSON with customizable NAND parameters."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


# Default timing parameters (in microseconds)
DEFAULT_READ_TIME_US = 50
DEFAULT_PROGRAM_TIME_US = 600
DEFAULT_ERASE_TIME_US = 3000
DEFAULT_CONTROLLER_OVERHEAD_US = 5
DEFAULT_RNG_SEED = 42


def _parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
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


def create_config(
    dies: int, blocks_per_die: int, pages_per_block: int, page_size_bytes: int
) -> dict[str, Any]:
    """
    Create simulator configuration dictionary.

    Args:
        dies: Number of NAND dies
        blocks_per_die: Number of blocks per die
        pages_per_block: Number of pages per block
        page_size_bytes: Page size in bytes

    Returns:
        Configuration dictionary
    """
    return {
        "nand": {
            "dies": dies,
            "blocks_per_die": blocks_per_die,
            "pages_per_block": pages_per_block,
            "page_size_bytes": page_size_bytes,
            "timing": {
                "t_read_us": DEFAULT_READ_TIME_US,
                "t_prog_us": DEFAULT_PROGRAM_TIME_US,
                "t_erase_us": DEFAULT_ERASE_TIME_US,
            },
        },
        "controller": {"ctrl_overhead_us": DEFAULT_CONTROLLER_OVERHEAD_US},
        "rng_seed": DEFAULT_RNG_SEED,
    }


def write_config(config: dict[str, Any], output_path: Path) -> None:
    """
    Write configuration to JSON file.

    Args:
        config: Configuration dictionary
        output_path: Path to output JSON file
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as handle:
        json.dump(config, handle, indent=2)
        handle.write("\n")
    print(f"[OK] Wrote {output_path}")


def main() -> None:
    """Main entry point."""
    args = _parse_args()
    config = create_config(args.dies, args.blocks, args.ppb, args.psize)
    write_config(config, Path(args.out))


if __name__ == "__main__":
    main()
