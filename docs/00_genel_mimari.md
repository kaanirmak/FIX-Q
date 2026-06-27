# FIX-Q Projesi - Genel Mimari Dökümanı

Bu döküman, **FIX-Q (Finora Quantum-Safe HFT Terminal)** projesinin genel sistem mimarisini, veri akışını, entegrasyon şemalarını ve kullanılan teknolojileri kapsamaktadır.

---

## 1. Özet Paragrafı
**FIX-Q**, Yüksek Frekanslı İşlem (HFT) sistemlerinde yaygın olarak kullanılan **FIX (Financial Information eXchange)** protokolü trafiğinin, kuantum bilgisayarların yaratacağı tehditlere karşı **Kuantum Sonrası Şifreleme (PQC - Post-Quantum Cryptography)** yöntemleriyle korunmasını simüle eden ve analiz eden hibrit bir güvenlik platformudur. Proje, klasik **TLS 1.3** protokolü ile FIPS-203/204 standartlarında yer alan hibrit PQC (ML-KEM-768 ve ML-DSA-65) tünelleme yaklaşımlarını eşzamanlı olarak çalıştırarak; şifreleme gecikmeleri (latency), CPU döngüleri (CPU cycles) ve ağ paket parçalanması (fragmentation) parametreleri üzerinden mikro saniye hassasiyetinde bir performans karşılaştırması sunar.

---

## 2. Mimari Şeması
Aşağıdaki diyagram, FIX-Q projesinin bileşenlerini ve veri yollarını göstermektedir:

```mermaid
graph TD
    subgraph ClientSpace ["Client Space (İstemci Alanı)"]
        Client[Plaintext FIX Client]
        WebUI[Web Browser Dashboard - Port 8080]
    end

    subgraph SecurityTunnel ["Security Tunnel Layer (Güvenlik Tüneli Katmanı)"]
        subgraph TLSSuite ["TLS Suite"]
            TLSEntry[TLS Client Entry Server - Port 5007]
            TLSDecrypt[TLS Decrypt Server - Port 5008]
        end
        PQCProxy[PQC Proxy Server - Port 5006]
    end

    subgraph ExchangeSim ["Exchange Simulator (Borsa Simülatörü)"]
        MockBist[Mock BIST Server - Port 5003]
    end

    subgraph MonitoringOrch ["Monitoring & Orchestration (İzleme ve Yönetim)"]
        WebServer[C++ Web Server - Port 8080]
        PyCrypto[Python Cryptographic Engine]
        Bench[C++ Benchmark Engine]
    end

    %% Web UI Connections
    WebUI <-->|HTTP API / static assets| WebServer
    WebServer <-->|Executes helper| PyCrypto
    WebServer -->|Measures live latencies| TLSEntry
    WebServer -->|Measures live latencies| PQCProxy
    WebServer -->|Triggers| Bench

    %% Data Tunnels
    Client -->|Plaintext FIX| TLSEntry
    Client -->|Plaintext FIX| PQCProxy

    TLSEntry -->|TLS 1.3 Encrypted FIX| TLSDecrypt
    TLSDecrypt -->|Decrypted Plaintext FIX| MockBist

    PQCProxy -->|Simulated PQC Encrypted FIX| MockBist
    
    Bench -->|Bulk test data| TLSEntry
    Bench -->|Bulk test data| PQCProxy
```

---

## 3. Akış Şeması
Aşağıdaki akış şeması, sisteme gönderilen manuel bir emrin şifrelenmesi, iletilmesi, işlenmesi ve geri dönen yanıtın analiz edilerek kullanıcı arayüzüne yansıtılma sürecini göstermektedir:

```mermaid
sequenceDiagram
    autonumber
    actor User as "Kullanıcı (Tarayıcı)"
    participant WS as "Web Server (Port 8080)"
    participant TP as "TLS Proxy Suite (Port 5007)"
    participant PP as "PQC Proxy Server (Port 5006)"
    participant BIST as "Mock BIST (Port 5003)"
    participant PY as "Python Crypto Script"

    User->>WS: Post Order (Symbol, Qty, Price)
    Note over WS: FIX NewOrderSingle (35=D) oluşturulur.
    
    par TLS Tünel Ölçümü
        WS->>TP: Plaintext FIX Gönder
        Note over TP: TLS Handshake & AES-GCM encryption
        TP->>BIST: Decrypted Plaintext ilet
        BIST-->>TP: Execution Report (35=8)
        TP-->>WS: Plaintext Response + Gecikme (ms)
    and PQC Tünel Ölçümü
        WS->>PP: Plaintext FIX Gönder
        Note over PP: ML-KEM / ML-DSA simüle gecikme enjeksiyonu
        PP->>BIST: Plaintext ilet
        BIST-->>PP: Execution Report (35=8)
        PP-->>WS: Plaintext Response + Gecikme (ms)
    end

    WS->>PY: Hex(FIX Msg) + TLS Latency + PQC Latency
    Note over PY: X25519, ML-KEM-768, AES-GCM ve ML-DSA-65 paket boyut kırılımlarını hesaplar.
    PY-->>WS: Detaylı Kripto Analiz JSON
    WS-->>User: JSON Verisi (Grafikler ve Paket Kırılımı)
```

---

## 4. Kullanılan Teknolojiler
Projenin genelinde kullanılan temel teknolojiler ve kütüphaneler şunlardır:

*   **C++17**: Performans kritik proxy motorları, eşleştirme motoru ve web sunucusunun geliştirilmesinde kullanılmıştır. Low-latency HFT optimizasyonları için tercih edilmiştir.
*   **Python 3**: Kuantum sonrası kriptografi simülasyonu, veri analitiği, paket parçalanma hesaplamaları ve FIX test verisi üretimi amacıyla kullanılmıştır.
*   **OpenSSL (v3.x)**: TLS tünelleme katmanında SSL/TLS el sıkışması ve PQC tünelindeki gerçek simetrik şifreleme işlemlerinde (AES-256-GCM) kullanılan temel kriptografi kütüphanesidir.
*   **Mermaid.js**: Mimarilerin ve akış şemalarının görselleştirilmesinde kullanılan diyagram aracıdır.
*   **POSIX Sockets**: Yüksek performanslı TCP bağlantı yönetimi için kullanılmıştır.
