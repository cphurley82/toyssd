import json, argparse
p = argparse.ArgumentParser()
p.add_argument("--dies", type=int, default=2)
p.add_argument("--blocks", type=int, default=256)
p.add_argument("--ppb", type=int, default=128)
p.add_argument("--psize", type=int, default=4096)
p.add_argument("--out", default="config/generated.json")
args = p.parse_args()

cfg = {
  "nand": {
    "dies": args.dies,
    "blocks_per_die": args.blocks,
    "pages_per_block": args.ppb,
    "page_size_bytes": args.psize,
    "timing": {"t_read_us":50,"t_prog_us":600,"t_erase_us":3000}
  },
  "controller": {"ctrl_overhead_us":5},
  "rng_seed": 42
}
with open(args.out, "w") as f:
    json.dump(cfg, f, indent=2)
print("[OK] Wrote", args.out)
