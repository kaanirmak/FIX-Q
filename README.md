flowchart TD
    %% Renk ve Tema Tanımları
    classDef client fill:#0f172a,stroke:#38bdf8,stroke-width:2px,color:#f8fafc;
    classDef proxy fill:#1e1b4b,stroke:#8b5cf6,stroke-width:2px,color:#c4b5fd;
    classDef internal fill:#312e81,stroke:#6366f1,stroke-width:1px,color:#e0e7ff;
    classDef internet fill:#450a0a,stroke:#dc2626,stroke-width:2px,color:#fecaca;
    classDef server fill:#0f172a,stroke:#10b981,stroke-width:2px,color:#f8fafc;
    classDef ui fill:#022c22,stroke:#34d399,stroke-width:1px,color:#ecfdf5;

    %% 1. İstemci Katmanı
    subgraph ClientLayer ["🏢 Aracı Kurum / Banka Ağı (LAN)"]
        HFT[("HFT Algoritması\n(FIX 4.4 İstemcisi)")]:::client
        UI["Web Dashboard\n(Aiohttp + WebSocket)\nPort: 8080"]:::ui
    end

    %% 2. İstemci Proxy Katmanı
    subgraph ClientProxyLayer ["🛡️ FIX-Q İstemci Proxy Katmanı"]
        CP_Listener("TCP Listener\n(Port: 5001)"):::internal
        CP_Parser("core/parser.py\n(Hızlı Bayt Ayrıştırıcı - Tag 35)"):::internal
        CP_Crypto("crypto/pqc_tunnel.py\n(ML-KEM 768 + ML-DSA 65)"):::internal
        CP_Writer("Async uvloop Writer"):::internal

        CP_Listener --> |Ham FIX Baytları| CP_Parser
        CP_Parser --> |Parsed Dict| CP_Crypto
        CP_Crypto --> |Şifreli Payload| CP_Writer
    end

    HFT -- "TCP (Düz Metin FIX)\n8=FIX.4.4|9=80|35=D..." --> CP_Listener

    %% 3. İnternet (WAN) Katmanı
    subgraph InternetLayer ["🌐 Geniş Alan Ağı (WAN / Internet)"]
        HNDL_Threat{{"Kuantum Korsanı\n(Harvest Now, Decrypt Later)"}}:::internet
        Tunnel(("Güvenli PQC Tüneli\n(Uzunluk Ön-Ekli TCP Çerçeveleri)")):::proxy
        
        HNDL_Threat -.->|Saldırı / Dinleme| Tunnel
    end

    CP_Writer ==> |Şifreli Veri| Tunnel

    %% 4. Sunucu Proxy Katmanı
    subgraph ServerProxyLayer ["🛡️ FIX-Q Sunucu Proxy Katmanı"]
        SP_Listener("TCP Listener\n(Port: 5002)"):::internal
        SP_Crypto("crypto/pqc_tunnel.py\n(Şifre Çözme ve İmza Doğrulama)"):::internal
        SP_Writer("Async uvloop Writer"):::internal

        SP_Listener --> |Şifreli Payload| SP_Crypto
        SP_Crypto --> |Doğrulanmış Düz Metin| SP_Writer
    end

    Tunnel ==> SP_Listener

    %% 5. Borsa / Sunucu Katmanı
    subgraph BISTLayer ["🏦 Borsa İstanbul (BIST / Hedef Sunucu)"]
        BIST[("BIST FIX Gateway\n(Port: 5003)")]:::server
    end

    SP_Writer -- "TCP (Düz Metin FIX)\n8=FIX.4.4|9=80|35=D..." --> BIST

    %% 6. Telemetri ve Loglama
    CP_Parser -.-> |Trace Event: STAGE_1_SENT| UI
    CP_Crypto -.-> |Trace Event: STAGE_2_ENCRYPTED| UI
    SP_Listener -.-> |Trace Event: STAGE_3_RECEIVED| UI
    SP_Writer -.-> |Trace Event: STAGE_4_DELIVERED| UI

Bileşen Detayları
1. HFT & Client Layer
Mevcut aracı kurum veya bankanın altyapısıdır. Hiçbir koda dokunulmadan, FIX motorlarının hedef IP adresi (BIST yerine localhost:5001) olarak değiştirilmesiyle sisteme entegre olur ("Zero-Downtime Integration").

2. Core Parser (core/parser.py)
QuickFIX gibi ağır (ve yavaş) kütüphaneler kullanılmamıştır. Python'da özel yazılan bayt seviyesinde SOH (\x01) tarayıcısı, saniyede +10.000 paketi mikrosaniyeler içinde ayrıştırır ve sadece emir tipini (Tag 35) okur.

3. PQC Tunnel (crypto/pqc_tunnel.py)
Simetrik ve Asimetrik kriptografinin Post-Quantum versiyonudur.

Kapsülleme (KEM): ML-KEM (Kyber) kullanılarak anahtar takası yapılır. Shor algoritmasına (Kuantum Bilgisayarlara) karşı dirençlidir.
İmza (DSA): ML-DSA (Dilithium) kullanılarak emrin BIST'e gönderilmeden önce bozulmadığından ve kaynağının doğru olduğundan (Non-repudiation) emin olunur.
4. Async I/O Motoru (uvloop & asyncio)
Sistemin HFT (High-Frequency Trading) şartlarına dayanabilmesi için standart Python CPython olay döngüsü yerine, Node.js ve Go gibi dillerin arkasındaki asenkron C-kütüphanesi olan libuv tabanlı uvloop kullanılmıştır. Bu sayede soket okuma/yazma işlemleri bloklanmadan (non-blocking) gerçekleşir.
