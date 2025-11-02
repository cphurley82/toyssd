# Placeholder plotting script (extend once logs are produced)
import argparse

import matplotlib.pyplot as plt
import pandas as pd


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot common metrics from simulator CSV output.")
    parser.add_argument("--csv", required=True, help="Path to CSV file to plot.")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    df = pd.read_csv(args.csv)
    for col in ["latency_us"]:
        if col in df.columns:
            df[col].plot(kind="hist", bins=50, title=col)
            plt.show()


if __name__ == "__main__":
    main()
