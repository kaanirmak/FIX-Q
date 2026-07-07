# FIX-Q: Finora Quantum-Safe HFT Terminal

FIX-Q, Yüksek Frekanslı İşlem (HFT) sistemlerinde kullanılan **FIX (Financial Information eXchange)** protokolü trafiğinin, Kuantum Sonrası Şifreleme (PQC - Post-Quantum Cryptography) yöntemleriyle korunmasını simüle eden ve analiz eden hibrit bir güvenlik platformudur.

Bu proje, FIPS-203/204 standartlarında yer alan hibrit PQC (ML-KEM-768 ve ML-DSA-65) tünelleme yaklaşımlarını, klasik **TLS 1.3** protokolüyle eş zamanlı çalıştırarak; şifreleme gecikmeleri, CPU döngüleri ve ağ paket parçalanması parametreleri üzerinden performans karşılaştırmaları sunar.

---

## 📚 Detaylı Proje Dökümantasyonu

Projenin her bir bileşeni için hazırlanan detaylı analiz dökümanlarına, mimari şemalarına ve akış diyagramlarına aşağıdaki bağlantılardan ulaşabilirsiniz:

1.  **[Genel Sistem Mimarisi](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/00_genel_mimari.md)**: Projenin genel amacı, uçtan uca veri yolları ve entegrasyon modeli.
2.  **[Mock BIST Eşleştirme Motoru](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/01_mock_bist.md)**: BIST borsasını simüle eden TCP FIX sunucusu.
3.  **[TLS 1.3 Tüneli (TLS Proxy Suite)](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/02_tls_proxy.md)**: OpenSSL tabanlı klasik TLS 1.3 şifreleme ve deşifreleme proksileri.
4.  **[C++ PQC Proxy ve Ağ Simülatörü](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/03_pqc_proxy.md)**: Kuantum sonrası anahtar ve imza boyutları nedeniyle ağ paket parçalanmasını simüle eden C++ proxy.
5.  **[Kriptografik PQC Tünel Motoru](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/04_pqc_crypto_engine.md)**: Hibrit PQC (X25519 + ML-KEM + AES-GCM + ML-DSA) paket paketleme formatı ve byte dökümü.
6.  **[Web Sunucusu ve Dashboard Arayüzü](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/05_web_server_ui.md)**: httplib.h tabanlı web backend ve tarayıcı tabanlı premium arayüz terminali.
7.  **[Performans Test ve Benchmark Araçları](file:///Users/kaanirmak/Documents/GitHub/FIX-Q/docs/06_benchmark_tools.md)**: Toplu işlem test aracı ve donanım düzeyinde CPU döngü sayacı (RDTSC).

---

## 🚀 Hızlı Başlangıç

### Gereksinimler
*   C++17 destekli bir derleyici (g++ veya clang)
*   OpenSSL 3.x (Homebrew ile kurulabilir)
*   Python 3.x ve `cryptography`, `pyyaml` paketleri

### Kurulum ve Çalıştırma
Sistemdeki tüm servisleri derlemek ve arka planda çalıştırmak için sağlanan kabuk betiğini kullanabilirsiniz:

```bash
chmod +x start_prod.sh status.sh stop_prod.sh
./start_prod.sh
```

Servislerin durumunu kontrol etmek için:
```bash
./status.sh
```

Arayüze erişmek için tarayıcınızdan şu adrese gidin:
```
http://localhost:8080
```

Servisleri durdurmak için:
```bash
./stop_prod.sh
```
