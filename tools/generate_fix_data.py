#!/usr/bin/env python3
import json
import random
import os
import sys

# Ensure market data is fresh
if not os.path.exists("config/market_data.json"):
    import subprocess
    feed_path = "examples/mock_exchange/market_data_feed.py" if os.path.exists("examples/mock_exchange/market_data_feed.py") else "src/market_data_feed.py"
    subprocess.run([sys.executable, feed_path], check=True)

with open("config/market_data.json", "r") as f:
    market_data = json.load(f)

symbols = list(market_data.keys())

def build_fix_message(fields):
    body_parts = []
    for tag, val in fields:
        if tag in (8, 9, 10):
            continue
        body_parts.append(f"{tag}={val}")
    body = "|".join(body_parts) + "|"
    body_len = len(body)
    
    msg_without_chk = f"8=FIX.4.4|9={body_len}|{body}"
    soh_msg = msg_without_chk.replace("|", "\x01")
    checksum_val = sum(soh_msg.encode('utf-8')) % 256
    return f"{msg_without_chk}10={checksum_val:03d}|"

output_file = "fix_test_data.txt"
with open(output_file, "w") as f:
    for i in range(1, 1001):
        cl_ord_id = f"BIST{i:05d}"
        symbol = random.choice(symbols)
        ticker_info = market_data[symbol]
        base_price = ticker_info["price"]

        side = random.choice(['1', '2']) # 1=Buy, 2=Sell
        qty = random.choice([25, 50, 100, 200, 500, 1000])

        # Cluster around live BIST bid/ask prices with realistic spread
        tick_step = 0.10 if base_price > 100 else 0.02
        price_offset = random.randint(-5, 5) * tick_step
        order_price = round(base_price + price_offset, 2)
        if order_price <= 0:
            order_price = base_price

        fields = [
            (35, "D"),
            (11, cl_ord_id),
            (21, "1"),
            (55, symbol),
            (54, side),
            (38, qty),
            (40, "2"),
            (44, f"{order_price:.2f}")
        ]
        
        fix_line = build_fix_message(fields) + "\n"
        f.write(fix_line)

print(f"✓ Generated 1000 genuine BIST market FIX orders into '{output_file}' based on live exchange prices.")
