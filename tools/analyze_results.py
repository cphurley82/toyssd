"""Analyze simulator output and generate latency statistics and histograms."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def _parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description="Basic analysis helper for simulator CSV output.")
    parser.add_argument("--input", required=True, help="Path to CSV file to analyze.")
    return parser.parse_args()


def analyze_csv(csv_path: Path) -> None:
    """
    Analyze simulation results from CSV and display histogram.

    Args:
        csv_path: Path to the CSV file containing simulation results
    """
    df = pd.read_csv(csv_path)
    print(df.describe())

    if "latency_us" not in df.columns:
        print("Warning: 'latency_us' column not found in CSV")
        return

    df["latency_us"].hist(bins=50)
    plt.xlabel("Latency (us)")
    plt.ylabel("Count")
    plt.title("Latency Histogram")
    plt.show()


def main() -> None:
    """Main entry point."""
    args = _parse_args()
    analyze_csv(Path(args.input))


if __name__ == "__main__":
    main()
