import argparse, pandas as pd, matplotlib.pyplot as plt

# Placeholder: expects a CSV produced by future logging
parser = argparse.ArgumentParser()
parser.add_argument("--input", required=True)
args = parser.parse_args()

df = pd.read_csv(args.input)
print(df.describe())
df['latency_us'].hist(bins=50)
plt.xlabel("Latency (us)"); plt.ylabel("Count"); plt.title("Latency Histogram")
plt.show()
