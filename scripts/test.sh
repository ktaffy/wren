#!/usr/bin/env bash
set -euo pipefail

PYTHON="${PYTHON:-python3}"
PORT=8080
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SERVER="$ROOT/c/build/release/server"
LOG="$ROOT/c/build/server.log"

echo "== C unit tests =="
make -C "$ROOT/c" test

port_open() {
    "$PYTHON" -c "import socket; socket.create_connection(('127.0.0.1', $PORT), timeout=0.2).close()" 2>/dev/null
}

if port_open; then
    echo "error: port $PORT is already in use" >&2
    exit 1
fi

"$SERVER" >"$LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

ready=0
for _ in $(seq 50); do
    if port_open; then
        ready=1
        break
    fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "error: server exited during startup; log follows" >&2
        cat "$LOG" >&2
        exit 1
    fi
    sleep 0.1
done
if [ "$ready" -ne 1 ]; then
    echo "error: server did not start listening on port $PORT" >&2
    exit 1
fi

echo "== wrengen tests =="
(cd "$ROOT/wrengen" && "$PYTHON" -m unittest -v)

echo "== python client tests =="
(cd "$ROOT/python" && "$PYTHON" -m unittest discover -s tests -v)

echo "== all tests passed =="