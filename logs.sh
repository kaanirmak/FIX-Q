#!/bin/bash
# Finora-Q Live Colorized Log Streamer
# Usage:
#   ./logs.sh          (Stream all active logs combined)
#   ./logs.sh bist     (Stream Mock BIST matching engine logs)
#   ./logs.sh pqc      (Stream PQC Proxy tunnel logs)
#   ./logs.sh tls      (Stream TLS 1.3 Proxy logs)
#   ./logs.sh test     (Stream Test Engine logs)
#   ./logs.sh grafana  (Stream Grafana logs)

cd "$(dirname "$0")"

TARGET=${1:-"all"}

mkdir -p logs
touch logs/mock_bist.log logs/pqc_proxy.log logs/tls_proxy.log logs/test_engine.log logs/grafana.log logs/prometheus.log

case $TARGET in
    bist|mock_bist)
        echo "📡 Streaming Mock BIST logs (logs/mock_bist.log)... [Ctrl+C ile çıkın]"
        tail -n 50 -f logs/mock_bist.log
        ;;
    pqc|pqc_proxy)
        echo "🛡️ Streaming PQC Proxy logs (logs/pqc_proxy.log)... [Ctrl+C ile çıkın]"
        tail -n 50 -f logs/pqc_proxy.log
        ;;
    tls|tls_proxy)
        echo "🔒 Streaming TLS 1.3 Proxy logs (logs/tls_proxy.log)... [Ctrl+C ile çıkın]"
        tail -n 50 -f logs/tls_proxy.log
        ;;
    test|test_engine)
        echo "⚡ Streaming Test Engine logs (logs/test_engine.log)... [Ctrl+C ile çıkın]"
        tail -n 50 -f logs/test_engine.log
        ;;
    grafana)
        echo "📊 Streaming Grafana logs (logs/grafana.log)... [Ctrl+C ile çıkın]"
        tail -n 50 -f logs/grafana.log
        ;;
    prom|prometheus)
        echo "📈 Streaming Prometheus logs (logs/prometheus.log)... [Ctrl+C ile çıkın]"
        tail -n 50 -f logs/prometheus.log
        ;;
    all|*)
        echo "=========================================================="
        echo "🌟 Finora-Q Tüm Servislerin Canlı Log Akışı (Multiplexed)"
        echo "   BIST | PQC Gateway | TLS Proxy | Test Engine"
        echo "=========================================================="
        echo "Durdurmak için Ctrl+C tuşlarına basın..."
        echo ""

        python3 -c '
import sys, time, glob, os

files = {
    "BIST": "logs/mock_bist.log",
    "PQC ": "logs/pqc_proxy.log",
    "TLS ": "logs/tls_proxy.log",
    "TEST": "logs/test_engine.log"
}

colors = {
    "BIST": "\033[96m",   # Cyan
    "PQC ": "\033[95m",   # Magenta/Purple
    "TLS ": "\033[94m",   # Blue
    "TEST": "\033[92m",   # Green
    "RESET": "\033[0m",
    "WARN": "\033[93m",   # Yellow
    "ERR": "\033[91m",    # Red
}

# Print recent history first
recent_lines = []
handles = {}
for tag, path in files.items():
    if os.path.exists(path):
        f = open(path, "r", errors="replace")
        all_l = f.readlines()
        for l in all_l[-15:]:
            c = l.strip()
            if c:
                recent_lines.append((tag, c))
        handles[tag] = f

def sort_key(item):
    l = item[1]
    if l.startswith("[202") and len(l) > 23:
        return l[1:24]
    return ""

recent_lines.sort(key=sort_key)
for tag, clean in recent_lines[-20:]:
    c = colors.get(tag, "")
    reset = colors["RESET"]
    lvl_highlight = ""
    if "WARN" in clean:
        lvl_highlight = colors["WARN"]
    elif "ERROR" in clean or "Fatal" in clean:
        lvl_highlight = colors["ERR"]
    elif "Order Matched!" in clean:
        lvl_highlight = "\033[92m"
    print(f"{c}[{tag}]{reset} {lvl_highlight}{clean}{reset}")

print("\n✓ Canlı akış aktif. Yeni gelen loglar anlık renklendiriliyor [Durdurmak için Ctrl+C]:\n")

try:
    while True:
        had_data = False
        for tag, f in handles.items():
            line = f.readline()
            while line:
                had_data = True
                clean = line.strip()
                c = colors.get(tag, "")
                reset = colors["RESET"]
                
                # Highlight level & matches
                lvl_highlight = ""
                if "WARN" in clean:
                    lvl_highlight = colors["WARN"]
                elif "ERROR" in clean or "Fatal" in clean:
                    lvl_highlight = colors["ERR"]
                elif "Order Matched!" in clean:
                    lvl_highlight = "\033[92m"
                    
                print(f"{c}[{tag}]{reset} {lvl_highlight}{clean}{reset}", flush=True)
                line = f.readline()
                
        if not had_data:
            time.sleep(0.15)
except KeyboardInterrupt:
    print("\nLog akışı durduruldu.")
'
        ;;
esac
