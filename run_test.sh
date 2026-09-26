#!/bin/bash
# FINORA-Q Multi-Domain Test Runner (CLI)
# Usage:
#   ./run_test.sh borsa [tps] [duration_seconds]
#   ./run_test.sh banking [tps] [duration_seconds]
#   ./run_test.sh web3 [tps] [duration_seconds]
#   ./run_test.sh all [tps] [duration_seconds]

cd "$(dirname "$0")"

DOMAIN=${1:-"borsa"}
TPS=${2:-100}
DURATION=${3:-15}

API_URL="http://127.0.0.1:9100/api/test/start"

# Check if test engine is running
if ! curl -s "http://127.0.0.1:9100/api/status" > /dev/null 2>&1; then
    echo "⚠️  Test Engine (port 9100) is not running. Starting all services first..."
    ./start_all.sh
    sleep 2
fi

run_domain() {
    local target_dom=$1
    local target_tps=$2
    local target_dur=$3

    echo ""
    echo "=========================================================="
    echo "🚀 Starting $target_dom Test ($target_tps TPS for ${target_dur}s)..."
    echo "=========================================================="

    RESPONSE=$(curl -s -X POST "$API_URL" \
      -H "Content-Type: application/json" \
      -d "{\"domain\":\"$target_dom\",\"tps\":$target_tps,\"duration\":$target_dur,\"security\":\"both\"}")

    echo "API Response: $RESPONSE"
    echo ""
    echo "📊 Grafana Canlı Dashboard Bağlantıları:"
    case $target_dom in
        borsa)
            echo "👉 Borsa Paneli: http://localhost:3000/d/fixq-borsa"
            ;;
        banking)
            echo "👉 Bankacılık Paneli: http://localhost:3000/d/fixq-banking"
            ;;
        web3)
            echo "👉 Web3 Paneli: http://localhost:3000/d/fixq-web3"
            ;;
    esac
    echo "👉 Genel Karşılaştırma Paneli: http://localhost:3000/d/fixq-overview"
    echo ""
    echo "Test devam ediyor, canlı ilerleme izleniyor..."

    local end_time=$((SECONDS + target_dur + 2))
    while [ $SECONDS -lt $end_time ]; do
        STATUS=$(curl -s "http://127.0.0.1:9100/api/status")
        RUNNING=$(echo "$STATUS" | python3 -c "import sys, json; print(json.load(sys.stdin).get('is_running', False))" 2>/dev/null || echo "False")
        
        if [ "$RUNNING" == "False" ] && [ $SECONDS -gt $((end_time - target_dur + 3)) ]; then
            break
        fi

        CURRENT_TPS=$(echo "$STATUS" | python3 -c "import sys, json; print(json.load(sys.stdin)['throughput_tps'].get('$target_dom', 0))" 2>/dev/null || echo "0")
        echo -ne "\r⏳ Kalan Süre: $((end_time - SECONDS))s | Anlık TPS: $CURRENT_TPS | Metrikler Grafana'ya akıyor... "
        sleep 1
    done

    echo -e "\n✓ $target_dom testi başarıyla tamamlandı!\n"
}

if [ "$DOMAIN" == "all" ]; then
    echo "🌟 Borsa, Bankacılık ve Web3 testleri ardışık olarak başlatılıyor..."
    run_domain "borsa" $TPS $DURATION
    sleep 2
    run_domain "banking" $TPS $DURATION
    sleep 2
    run_domain "web3" $TPS $DURATION
    echo "=========================================================="
    echo "🎉 Tüm domain testleri tamamlandı!"
    echo "📊 Genel Karşılaştırma Grafana: http://localhost:3000/d/fixq-overview"
    echo "=========================================================="
else
    run_domain "$DOMAIN" $TPS $DURATION
fi
