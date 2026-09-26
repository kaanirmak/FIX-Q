#!/bin/bash

cd "$(dirname "$0")"

echo "Shutting down FINORA-Q & Grafana Stack..."

kill_from_pid() {
    if [ -f "logs/$1.pid" ]; then
        PID=$(cat "logs/$1.pid")
        if ps -p $PID > /dev/null 2>&1; then
            echo "Stopping $1 (PID: $PID)..."
            kill $PID 2>/dev/null
        fi
        rm -f "logs/$1.pid"
    fi
}

kill_from_pid "grafana"
kill_from_pid "prometheus"
kill_from_pid "test_engine"
kill_from_pid "web_server"
kill_from_pid "tls_proxy"
kill_from_pid "pqc_proxy"
kill_from_pid "mock_bist"

# Fallback killall for clean state
killall web_server pqc_proxy tls_proxy mock_bist >/dev/null 2>&1 || true
pkill -f "test_engine.py" >/dev/null 2>&1 || true
pkill -f "grafana server" >/dev/null 2>&1 || true
pkill -f "prometheus --config.file" >/dev/null 2>&1 || true

echo "✓ All FINORA-Q services and monitoring daemons stopped."
