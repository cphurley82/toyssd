import argparse

import matplotlib.pyplot as plt
import pandas as pd


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Basic analysis helper for simulator CSV output.")
    parser.add_argument("--input", required=True, help="Path to CSV file to analyze.")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    df = pd.read_csv(args.input)
    print(df.describe())
    df["latency_us"].hist(bins=50)
    plt.xlabel("Latency (us)")
    plt.ylabel("Count")
    plt.title("Latency Histogram")
    plt.show()


if __name__ == "__main__":
    main()
