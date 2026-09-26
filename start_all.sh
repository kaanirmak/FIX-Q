#!/bin/bash

echo "=========================================================="
echo "🚀 Starting Finora-Q Framework & Grafana Test Environment..."
echo "=========================================================="

cd "$(dirname "$0")"

mkdir -p logs
mkdir -p monitoring/data/prometheus
mkdir -p monitoring/data/grafana/plugins

# Stop any currently running instances
./stop_all.sh > /dev/null 2>&1

echo "1. Compiling Finora-Q Framework (-O3 -march=native -flto)..."
make prod full
if [ $? -ne 0 ]; then
    echo "❌ Build failed! Aborting startup."
    exit 1
fi

echo "2. Starting Mock BIST Matching Engine (Port 5003)..."
nohup ./bin/mock_bist > logs/mock_bist.log 2>&1 &
echo $! > logs/mock_bist.pid

echo "3. Starting Post-Quantum Cryptography Gateway (Port 5006)..."
nohup ./bin/pqc_proxy > logs/pqc_proxy.log 2>&1 &
echo $! > logs/pqc_proxy.pid

echo "4. Starting Classical TLS 1.3 Proxy Suite (Ports 5007 / 5008)..."
nohup ./bin/tls_proxy > logs/tls_proxy.log 2>&1 &
echo $! > logs/tls_proxy.pid

sleep 1

echo "5. Starting Multi-Domain Test Engine & Web Terminal (Ports 8080 & 9100)..."
nohup python3 -u tools/test_engine.py > logs/test_engine.log 2>&1 &
echo $! > logs/test_engine.pid

echo "6. Starting Prometheus Time-Series Server (Port 9090)..."
nohup /opt/homebrew/bin/prometheus \
    --config.file=monitoring/prometheus.yml \
    --storage.tsdb.path=monitoring/data/prometheus \
    --web.listen-address=0.0.0.0:9090 \
    > logs/prometheus.log 2>&1 &
echo $! > logs/prometheus.pid

echo "7. Starting Grafana Visualization Server (Port 3000)..."
nohup /opt/homebrew/opt/grafana/bin/grafana server \
    --config monitoring/grafana.ini \
    --homepath /opt/homebrew/opt/grafana/share/grafana \
    > logs/grafana.log 2>&1 &
echo $! > logs/grafana.pid

# Wait a moment for all ports to initialize
sleep 2

echo ""
echo "=========================================================="
echo "🎉 Finora-Q (Branch: Finora-Q) Services are RUNNING!"
echo "=========================================================="
echo "🖥️  Web UI Terminal:        http://localhost:8080 (veya http://localhost:9100)"
echo "📊 Grafana Dashboards:      http://localhost:3000"
echo "   ├── 📈 Borsa Test:       http://localhost:3000/d/fixq-borsa"
echo "   ├── 🏦 Bankacılık Test:  http://localhost:3000/d/fixq-banking"
echo "   ├── 🌐 Web3 Test:        http://localhost:3000/d/fixq-web3"
echo "   └── ⚖️  Genel Özet:       http://localhost:3000/d/fixq-overview"
echo "📈 Prometheus Server:       http://localhost:9090"
echo "⚡ Test Engine API:         http://localhost:9100/metrics"
echo ""
echo "📡 Canlı Log Akışı (Terminal):"
echo "   ./logs.sh                       # Tüm servislerin canlı renkli logları"
echo "   ./logs.sh bist                  # Yalnızca Mock BIST logları"
echo "   ./logs.sh pqc                   # Yalnızca PQC Proxy logları"
echo "   ./logs.sh test                  # Yalnızca Test Motoru logları"
echo ""
echo "📋 Test Komutları (CLI):"
echo "   ./run_test.sh borsa 100 15      # 100 TPS Borsa Testi"
echo "   ./run_test.sh banking 50 15     # 50 TPS Bankacılık Testi"
echo "   ./run_test.sh web3 80 15        # 80 TPS Web3 Testi"
echo "   ./run_test.sh all               # Tüm domain testleri"
echo "=========================================================="
