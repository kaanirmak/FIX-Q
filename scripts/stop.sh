#!/bin/bash

cd "$(dirname "$0")"

echo "Shutting down Finora HFT Prod Services..."

# Helper function to kill from pid file
kill_from_pid() {
    if [ -f "logs/$1.pid" ]; then
        PID=$(cat "logs/$1.pid")
        if ps -p $PID > /dev/null; then
            echo "Killing $1 (PID: $PID)..."
            kill $PID
        fi
        rm "logs/$1.pid"
    fi
}

kill_from_pid "web_server"
kill_from_pid "tls_proxy"
kill_from_pid "pqc_proxy"
kill_from_pid "mock_bist"

# Fallback killall just in case they were started manually without pid files
killall web_server pqc_proxy tls_proxy mock_bist >/dev/null 2>&1

echo "All services successfully stopped."
