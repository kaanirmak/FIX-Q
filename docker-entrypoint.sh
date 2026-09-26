#!/bin/bash
set -e

cd /app

mkdir -p logs /app/monitoring/data/prometheus /app/monitoring/data/grafana

# Railway dynamic port support (defaults to 3000 if not set)
HTTP_PORT=${PORT:-3000}

if [ "$1" = "start" ] || [ "$1" = "" ]; then
    echo "=========================================================="
    echo "🚀 Starting FINORA-Q Post-Quantum Framework (Railway / Cloud Mode)..."
    echo "=========================================================="

    # 1. Start Mock BIST Matching Engine (Port 5003)
    echo "1. Starting Mock BIST Matching Engine (Port 5003)..."
    ./bin/mock_bist > logs/mock_bist.log 2>&1 &
    echo $! > logs/mock_bist.pid

    # 2. Start PQC Gateway (Port 5006)
    echo "2. Starting Post-Quantum Cryptography Gateway (Port 5006)..."
    ./bin/pqc_proxy > logs/pqc_proxy.log 2>&1 &
    echo $! > logs/pqc_proxy.pid

    # 3. Start TLS Proxy (Ports 5007 / 5008)
    echo "3. Starting Classical TLS 1.3 Proxy Suite..."
    ./bin/tls_proxy > logs/tls_proxy.log 2>&1 &
    echo $! > logs/tls_proxy.pid

    sleep 1

    # 4. Start Multi-Domain Test Engine (Ports 8080 & 9100)
    echo "4. Starting Test Engine & Exporter (Ports 8080 & 9100)..."
    python3 -u tools/test_engine.py > logs/test_engine.log 2>&1 &
    echo $! > logs/test_engine.pid

    sleep 1

    # 5. Start Prometheus Server (Port 9090)
    echo "5. Starting Prometheus Server (Port 9090)..."
    prometheus \
        --config.file=/app/monitoring/prometheus.yml \
        --storage.tsdb.path=/app/monitoring/data/prometheus \
        --web.listen-address=0.0.0.0:9090 \
        > logs/prometheus.log 2>&1 &
    echo $! > logs/prometheus.pid

    sleep 1

    # 6. Start Grafana Visualization Server on Public $PORT
    echo "6. Starting Grafana Server on Port $HTTP_PORT..."
    export GF_SERVER_HTTP_PORT=$HTTP_PORT
    export GF_SERVER_HTTP_ADDR="0.0.0.0"
    export GF_SECURITY_ADMIN_USER="admin"
    export GF_SECURITY_ADMIN_PASSWORD="admin"
    export GF_AUTH_ANONYMOUS_ENABLED="true"
    export GF_AUTH_ANONYMOUS_ORG_ROLE="Admin"
    export GF_SECURITY_ALLOW_EMBEDDING="true"
    export GF_ANALYTICS_REPORTING_ENABLED="false"
    export GF_ANALYTICS_CHECK_FOR_UPDATES="false"
    export GF_PATHS_PROVISIONING="/app/monitoring/provisioning"
    export GF_PATHS_DATA="/app/monitoring/data/grafana"

    exec grafana-server \
        --config /app/monitoring/grafana.ini \
        --homepath /usr/share/grafana
else
    exec "$@"
fi
