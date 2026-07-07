#!/usr/bin/env bash
# Start the Python brain inside the drone container, wait for it to be healthy,
# then hand control to the C++ body. The body tolerates the brain being down
# (it falls back to a safe WAIT), but waiting here gives a clean startup.
set -uo pipefail

export PYTHONPATH=/app
PORT="${BRAIN_PORT:-7000}"

echo "[entrypoint] launching brain on 127.0.0.1:${PORT} (LLM=${BRAIN_USE_LLM:-0})"
uvicorn brain.main:app --host 127.0.0.1 --port "${PORT}" --log-level warning &

# Wait up to ~30s for the brain's /health endpoint.
for i in $(seq 1 30); do
    if python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:${PORT}/health', timeout=1)" >/dev/null 2>&1; then
        echo "[entrypoint] brain healthy after ${i}s"
        break
    fi
    sleep 1
done

# Hand off to the body (PID 1 so it receives docker stop / SIGTERM directly).
# When the body exits, the container stops and the brain goes with it.
exec ./main
