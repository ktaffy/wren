#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

SECS="${1:-10}"
WARMUP="${2:-2}"
RUNS="${3:-3}"
PORT=8080
OUT=build/results.csv

make -s

if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
    echo "error: port $PORT is already in use" >&2
    exit 1
fi

PIN_SERVER=()
PIN_CLIENT=()
if command -v taskset >/dev/null && [ "$(nproc)" -ge 2 ]; then
    PIN_SERVER=(taskset -c 0)
    PIN_CLIENT=(taskset -c "1-$(($(nproc) - 1))")
fi

"${PIN_SERVER[@]}" ./build/server >/dev/null 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT
for _ in $(seq 50); do
    (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null && break
    sleep 0.1
done

bench() {
    for _ in $(seq "$RUNS"); do
        "${PIN_CLIENT[@]}" ./build/loadgen -m "$1" -s "$2" -c "$3" \
            -d "$SECS" -w "$WARMUP"
    done
}

echo "method,size,conns,calls,secs,rps,p50_us,p99_us,max_us" > "$OUT"
for method in noop add; do
    for conns in 1 10 50 100; do
        bench "$method" 0 "$conns" | tee -a "$OUT"
    done
done
for size in 64 4096 65536 1048576; do
    bench echo "$size" 10 | tee -a "$OUT"
done

echo
echo "## Setup"
echo
echo "- CPU: $(lscpu | sed -n 's/^Model name: *//p')"
echo "- Cores: $(nproc), server pinned: ${PIN_SERVER[*]:-no}"
echo "- Kernel: $(uname -r)"
grep -qi microsoft /proc/version && echo "- Environment: WSL2 (virtualized and windows host shares the CPU)"
echo "- Compiler: $(${CC:-cc} --version | head -1)"
echo "- Each result: median of $RUNS runs, ${SECS}s measured after ${WARMUP}s warm-up, TCP loopback"
echo
python3 summarize.py "$OUT"