#!/usr/bin/env python3
import re
import subprocess
from pathlib import Path


def main():
    root = Path(__file__).resolve().parent
    bench_bin = root / "bench"

    if not bench_bin.exists():
        subprocess.run([str(root / "build.sh")], cwd=root, check=True)

    tests = [
        (4, "ShardCollector"),
        (5, "ThreadLocalCollector"),
        (6, "DoubleBufferCollector"),
    ]

    print(f"| {'Collector':<23} | {'Broken %':<10} | {'sum < count':<11} | {'sum > count':<11} | {'Diff':<8} |")
    print(f"| {':---':<23} | {':---:':<10} | {':---:':<11} | {':---:':<11} | {':---:':<8} |")

    for test_id, name in tests:
        proc = subprocess.run(
            [str(bench_bin), str(test_id), "-n", "4", "-s"],
            cwd=root,
            capture_output=True,
            text=True,
            check=True,
        )
        pct = float(re.search(r"\(([\d.]+)%\)", proc.stdout).group(1))
        less = int(re.search(r"sum\(buckets\) < count:\s*(\d+)", proc.stdout).group(1))
        more = int(re.search(r"sum\(buckets\) > count:\s*(\d+)", proc.stdout).group(1))
        ops = int(re.search(r"Total worker ops:\s*(\d+)", proc.stdout).group(1))
        cnt = int(re.search(r"Final count:\s*(\d+)", proc.stdout).group(1))
        diff = cnt - ops

        print(f"| {name:<23} | {pct:9.2f}% | {less:11d} | {more:11d} | {diff:8d} |")


if __name__ == "__main__":
    main()
