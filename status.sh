#!/bin/bash

cd "$(dirname "$0")"

echo "=== Finora HFT Prod Services Status ==="
check_service() {
    if [ -f "logs/$1.pid" ]; then
        PID=$(cat "logs/$1.pid")
        if ps -p $PID > /dev/null; then
            echo "[RUNNING] $1 (PID: $PID)"
        else
            echo "[DEAD]    $1 (PID: $PID file exists but process is gone)"
        fi
    else
        echo "[STOPPED] $1 (No PID file)"
    fi
}

check_service "mock_bist"
check_service "pqc_proxy"
check_service "tls_proxy"
check_service "web_server"
echo "======================================="
