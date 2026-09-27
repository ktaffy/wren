import csv
import statistics
import sys
from collections import defaultdict

def main(path):
    groups = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            groups[(row["method"], int(row["size"]), int(row["conns"]))].append(row)

    print("| Method | Payload | Conns | Throughput (req/s) | p50 (µs) | p99 (µs) |")
    print("|---|---:|---:|---:|---:|---:|")
    for (method, size, conns), rows in groups.items():
        med = lambda k: statistics.median(float(r[k]) for r in rows)  # noqa: E731
        payload = f"{size} B" if method == "echo" else "-"
        print(f"| {method} | {payload} | {conns} | {med('rps'):,.0f} "
              f"| {med('p50_us'):.1f} | {med('p99_us'):.1f} |")

if __name__ == "__main__":
    main(sys.argv[1])