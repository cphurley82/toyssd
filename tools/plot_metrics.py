"""Plot metrics from simulator CSV output with configurable columns."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def _parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description="Plot common metrics from simulator CSV output.")
    parser.add_argument("--csv", required=True, help="Path to CSV file to plot.")
    return parser.parse_args()


def plot_metrics(csv_path: Path, columns: list[str] | None = None) -> None:
    """
    Plot histogram metrics from CSV file.

    Args:
        csv_path: Path to the CSV file containing metrics
        columns: List of column names to plot (defaults to ["latency_us"])
    """
    if columns is None:
        columns = ["latency_us"]

    df = pd.read_csv(csv_path)

    for col in columns:
        if col in df.columns:
            df[col].plot(kind="hist", bins=50, title=col)
            plt.show()
        else:
            print(f"Warning: Column '{col}' not found in CSV")


def main() -> None:
    """Main entry point."""
    args = _parse_args()
    plot_metrics(Path(args.csv))


if __name__ == "__main__":
    main()
