#!/bin/bash

cd "$(dirname "$0")"

echo "=== Finora-Q & Grafana Multi-Domain Stack Status ==="

check_service() {
    local name=$1
    local port=$2
    local pid_file="logs/$name.pid"
    
    local is_running=false
    local pid=""

    if [ -f "$pid_file" ]; then
        pid=$(cat "$pid_file")
        if ps -p $pid > /dev/null 2>&1; then
            is_running=true
        fi
    fi

    # Check port listening
    local port_ok=false
    if lsof -i :$port >/dev/null 2>&1; then
        port_ok=true
    fi

    if [ "$is_running" = true ] || [ "$port_ok" = true ]; then
        printf "  %-22s [ONLINE]  (Port: %-5s PID: %s)\n" "$name" "$port" "$pid"
    else
        printf "  %-22s [OFFLINE] (Port: %-5s)\n" "$name" "$port"
    fi
}

check_service "mock_bist" "5003"
check_service "pqc_proxy" "5006"
check_service "tls_proxy" "5007"
check_service "test_engine" "9100"
check_service "web_dashboard" "8080"
check_service "prometheus" "9090"
check_service "grafana" "3000"

echo "--- Dedike Protokol Ağ Geçitleri (Farklı Portlar) ---"
echo "  [SOL: Klasik Güvenlik]"
check_service "borsa_klasik" "5011"
check_service "banking_klasik" "5012"
check_service "web3_klasik" "5013"
echo "  [SAĞ: Kuantum Güvenlik (PQC)]"
check_service "borsa_quantum" "5021"
check_service "banking_quantum" "5022"
check_service "web3_quantum" "5023"

echo "====================================================="
