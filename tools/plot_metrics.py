# Placeholder plotting script (extend once logs are produced)
import pandas as pd, matplotlib.pyplot as plt, argparse
p = argparse.ArgumentParser()
p.add_argument("--csv", required=True)
args = p.parse_args()
df = pd.read_csv(args.csv)
for col in ["latency_us"]:
    if col in df.columns:
        df[col].plot(kind="hist", bins=50, title=col)
        plt.show()
