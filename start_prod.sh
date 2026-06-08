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
make prod

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed. Aborting startup."
    exit 1
fi

echo "Build successful. Starting daemons..."

# Start background services with output redirected to specific log files
nohup ./bin/mock_bist > logs/mock_bist.log 2>&1 &
echo $! > logs/mock_bist.pid

nohup ./bin/pqc_proxy > logs/pqc_proxy.log 2>&1 &
echo $! > logs/pqc_proxy.pid

nohup ./bin/tls_proxy > logs/tls_proxy.log 2>&1 &
echo $! > logs/tls_proxy.pid

# Give proxies a moment to bind ports
sleep 1

# Start Web Server
nohup ./bin/web_server > logs/web_server.log 2>&1 &
echo $! > logs/web_server.pid

echo "========================================="
echo "Finora HFT Prod Services are RUNNING."
echo "Web Interface: http://localhost:8080"
echo "Logs are available in the logs/ directory."
echo "Use ./status.sh to check running processes."
echo "Use ./stop_prod.sh to gracefully shut down."
echo "========================================="
