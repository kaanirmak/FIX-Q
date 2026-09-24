#!/bin/bash

echo "Starting Finora HFT Production Environment..."

# Ensure we are in the correct directory
cd "$(dirname "$0")"

# Create logs directory
mkdir -p logs

# Clean up any previously running processes
./stop_prod.sh > /dev/null 2>&1

echo "Compiling with Prod Optimization (-O3 -march=native -flto)..."
make clean
make prod full

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed. Aborting startup."
    exit 1
fi

echo "Build successful. Initializing Real Market Data and Hardware Benchmarks..."
if [ -f "examples/mock_exchange/market_data_feed.py" ]; then
    python3 examples/mock_exchange/market_data_feed.py
else
    python3 src/market_data_feed.py
fi
./bin/micro_bench

echo "Starting background services..."
# Start BIST matching engine
nohup ./bin/mock_bist > logs/mock_bist.log 2>&1 &
echo $! > logs/mock_bist.pid

# Start PQC and TLS proxies
nohup ./bin/pqc_proxy > logs/pqc_proxy.log 2>&1 &
echo $! > logs/pqc_proxy.pid

nohup ./bin/tls_proxy > logs/tls_proxy.log 2>&1 &
echo $! > logs/tls_proxy.pid

# Give proxies a moment to bind ports
sleep 1.5

echo "Populating genuine dataset benchmarks over live TCP tunnels..."
if [ -f "tools/generate_fix_data.py" ]; then
    python3 tools/generate_fix_data.py
else
    python3 generate_fix_data.py
fi
./bin/benchmark -f fix_test_data.txt

echo "========================================="
echo "Finora PQC HFT Production Services are RUNNING."
echo "PQC Gateway Tunnel:  127.0.0.1:5006"
echo "TLS Fallback Bridge: 127.0.0.1:5007 / 5008"
echo "Matching Engine:     127.0.0.1:5003"
echo "Logs are available in the logs/ directory."
echo "Use ./status.sh to check running processes."
echo "Use ./stop_prod.sh to gracefully shut down."
echo "========================================="
