#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path
import plotly.graph_objects as go


def run_benchmark(bench_bin, cwd, test_id, name, threads):
    print(f"Running {name}...")
    results = []
    for t in threads:
        proc = subprocess.run(
            [str(bench_bin), str(test_id), "-n", str(t)],
            cwd=cwd,
            capture_output=True,
            text=True,
            check=True,
        )
        match = re.search(r"Result is:\s*([0-9.]+)\s*ops/ms", proc.stdout)
        if not match:
            sys.exit(f"Failed to parse result for {name} ({t} threads)")
        results.append((t, float(match.group(1))))
    return results


def main():
    root = Path(__file__).resolve().parent
    bench_bin = root / "bench"

    if not bench_bin.exists():
        subprocess.run([str(root / "build.sh")], cwd=root, check=True)

    threads = [1, 2, 4, 8, 16]
    naive = run_benchmark(bench_bin, root, 2, "NaiveCollector", threads)
    empty = run_benchmark(bench_bin, root, 3, "NaiveEmptyCollector", threads)

    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=[t for t, _ in naive],
        y=[val for _, val in naive],
        mode="lines+markers",
        name="NaiveCollector",
    ))
    fig.add_trace(go.Scatter(
        x=[t for t, _ in empty],
        y=[val for _, val in empty],
        mode="lines+markers",
        name="NaiveEmptyCollector",
    ))

    fig.update_layout(
        title="Comparison",
        xaxis=dict(title="Threads", tickvals=threads),
        yaxis=dict(title="Throughput (ops/ms)"),
        template="plotly_white",
    )

    fig.write_html(str(root / "naive_benchmark.html"))


if __name__ == "__main__":
    main()
