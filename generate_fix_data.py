import random

# BIST örnek hisse havuzu
symbols = ['GARAN', 'THYAO', 'ASELS', 'AKBNK', 'BIMAS', 'EREGL', 'KCHOL', 'TUPRS', 'SAHOL', 'SISE']

def generate_checksum(msg_str):
    # Standart FIX checksum hesabı (Sadece bilgi amaçlı, jeneratör için basit tutulabilir)
    # Bu projede ham string üzerinden 10=000| kalıbını bozmamak için örnekteki gibi sabit tutuyoruz.
    return "000"

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

with open("fix_test_data.txt", "w") as f:
    for i in range(1, 1001):
        cl_ord_id = f"ORDER{i:04d}"
        symbol = random.choice(symbols)
        side = random.choice(['1', '2']) # 1=Buy, 2=Sell
        qty = random.choice([10, 20, 50, 100, 200, 500, 1000])
        
        # Hisseye göre mantıklı fiyat aralıkları
        if symbol == 'THYAO':
            price = round(random.uniform(200.0, 320.0), 1)
        elif symbol == 'BIMAS':
            price = round(random.uniform(300.0, 450.0), 1)
        elif symbol == 'ASELS':
            price = round(random.uniform(40.0, 70.0), 1)
        else:
            price = round(random.uniform(10.0, 150.0), 1)
            
        fields = [
            (35, "D"),
            (11, cl_ord_id),
            (21, "1"),
            (55, symbol),
            (54, side),
            (38, qty),
            (40, "2"),
            (44, price)
        ]
        
        fix_line = build_fix_message(fields) + "\n"
        f.write(fix_line)

print("✓ 1000 satırlık 'fix_test_data.txt' dosyası başarıyla oluşturuldu.")
