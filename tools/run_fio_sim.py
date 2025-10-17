import argparse, subprocess, os, sys

parser = argparse.ArgumentParser()
parser.add_argument("--config", default="config/default.json")
parser.add_argument("--runtime", type=int, default=5)
args = parser.parse_args()

build_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build"))
fio = os.path.abspath(os.path.join(build_dir, "_deps", "fio-src", "fio"))
ioengine = os.path.abspath(os.path.join(build_dir, "libssdsim.so"))
config = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", args.config))

cmd = [
    fio,
    f"--ioengine={ioengine}",
    f"--filename={config}",
    "--name=demo",
    "--rw=randwrite",
    "--size=64M",
    "--bs=4k",
    "--iodepth=8",
    "--numjobs=1",
    "--time_based",
    f"--runtime={args.runtime}"
]

print("[INFO] Running:", " ".join(cmd))
sys.exit(subprocess.call(cmd))
