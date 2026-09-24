#!/usr/bin/env python3
import urllib.request
import json
import time
import os
import sys

SYMBOLS = [
    {"symbol": "THYAO", "name": "Türk Hava Yolları"},
    {"symbol": "GARAN", "name": "Garanti BBVA"},
    {"symbol": "ASELS", "name": "Aselsan"},
    {"symbol": "AKBNK", "name": "Akbank"},
    {"symbol": "BIMAS", "name": "BİM Mağazalar"},
    {"symbol": "EREGL", "name": "Erdemir"},
    {"symbol": "KCHOL", "name": "Koç Holding"},
    {"symbol": "TUPRS", "name": "Tüpraş"},
    {"symbol": "SAHOL", "name": "Sabancı Holding"},
    {"symbol": "SISE", "name": "Şişecam"}
]

HEADERS = {"User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36"}

def fetch_bist_ticker(ticker_info):
    sym = ticker_info["symbol"]
    url = f"https://query1.finance.yahoo.com/v8/finance/chart/{sym}.IS?interval=1d&range=1d"
    req = urllib.request.Request(url, headers=HEADERS)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            meta = data["chart"]["result"][0]["meta"]
            price = meta.get("regularMarketPrice", 0.0)
            prev_close = meta.get("chartPreviousClose", price)
            change = round(price - prev_close, 2)
            change_pct = round((change / prev_close) * 100, 2) if prev_close else 0.0
            volume = meta.get("regularMarketVolume", 0)

            # Realistic tight market spread based on standard BIST tick sizes
            spread = 0.10 if price > 100 else 0.02
            bid = round(price - (spread / 2.0), 2)
            ask = round(price + (spread / 2.0), 2)

            return {
                "symbol": sym,
                "name": ticker_info["name"],
                "price": price,
                "prev_close": prev_close,
                "change": change,
                "change_pct": change_pct,
                "bid": bid,
                "ask": ask,
                "volume": volume,
                "currency": "TRY",
                "updated_at": time.strftime("%Y-%m-%d %H:%M:%S")
            }
    except Exception as e:
        sys.stderr.write(f"Warning: Failed to fetch {sym}: {e}\n")
        return None

def update_market_cache(output_path="config/market_data.json"):
    results = {}
    for item in SYMBOLS:
        data = fetch_bist_ticker(item)
        if data:
            results[data["symbol"]] = data

    if not results and os.path.exists(output_path):
        return  # Keep old cache if network is down

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        json.dump(results, f, indent=2, ensure_ascii=False)
    print(f"Successfully refreshed {len(results)} live BIST tickers into {output_path}")

if __name__ == "__main__":
    out = "config/market_data.json"
    if len(sys.argv) > 1:
        out = sys.argv[1]
    update_market_cache(out)
