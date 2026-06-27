# FIX-Q - Performans Test ve Benchmark Araçları Dökümanı

Bu döküman, **Benchmark Engine** (`src/benchmark.cpp`), **Micro Benchmarking Tool** (`src/micro_bench.cpp`), ve **Test Veri Üreteci** (`generate_fix_data.py`) bileşenlerinin detaylı analizini, iç mimarisini, akış süreçlerini ve kullanılan teknolojilerini açıklar.

---

## 1. Özet Paragrafı
**Performans Test ve Benchmark Araçları**, FIX-Q platformunun kuantum sonrası ve klasik şifreleme altındaki gecikme parametrelerini mikro saniye hassasiyetinde ölçen ve bilimsel veri toplayan analiz araçları bütünüdür. C++ tabanlı **Benchmark Engine**, binlerce FIX emrini içeren veri setlerini okuyup, her bir emri ardışık olarak TLS (`Port 5007`) ve PQC (`Port 5006`) proksilerine göndererek gerçek ağ latanslarını ölçer. Bu verilerden p50 (Medyan), p90, p99 ve p99.9 (Kuyruk Latansı/Tail Latency) yüzdelik dilimlerini hesaplayıp kümülatif dağılım fonksiyonu (CDF) grafiklerini besler. Diğer bir araç olan **Micro Benchmarking Tool** ise donanım seviyesinde CPU döngü sayaçlarını okuyarak ML-KEM-768/X25519 (Anahtar Değişimi) ve ML-DSA-65/ECDSA (Dijital İmza) algoritmalarının işlemci üzerindeki ham hesaplama maliyetlerini (CPU cycles) ölçüp raporlar.

---

## 2. Mimari Şeması
Aşağıdaki diyagram, test araçlarının veri üreteçleri, proksiler ve ortaya çıkan veri dosyaları arasındaki veri akış ilişkilerini göstermektedir:

```mermaid
graph TD
    %% Veri Üretimi
    Gen[generate_fix_data.py] -->|1. Generate| DataFile[fix_test_data.txt / Uploaded Orders]
    
    %% Benchmark Engine
    subgraph Benchmark Suite
        Bench[benchmark.cpp]
        Micro[micro_bench.cpp]
    end

    %% Proksiler
    TLS[TLS Proxy Suite - Port 5007]
    PQC[PQC Proxy - Port 5006]

    %% Akış
    DataFile -->|2. Parse FIX Lines| Bench
    Bench -->|3. Measure TCP Latency| TLS
    Bench -->|3. Measure TCP Latency| PQC
    
    %% Çıktılar
    Bench -->|4. Export CSV| CSV[benchmark_results.csv]
    Bench -->|5. Export CDF Percentiles| TailJSON[tail_latency.json]
    
    %% Microbench
    Micro -->|6. Assembly level CPU Cycle Test| CPU[Hardware CPU Registers]
    Micro -->|7. Export Cycles| CpuJSON[cpu_cycles.json]
```

---

## 3. Akış Şeması
Aşağıdaki akış şemaları sırasıyla `benchmark.cpp` ve `micro_bench.cpp` araçlarının çalışma süreçlerini göstermektedir:

### A. Toplu Test (benchmark.cpp) Akışı:
```mermaid
flowchart TD
    Start1([Benchmark Başlatıldı]) --> ParseArgs[Argümanları Oku: -f dosya_yolu]
    ParseArgs --> LoadFile[Dosyayı Aç ve Her Satırı FIX Olarak Yükle]
    LoadFile --> LoopOrders{Her Bir FIX Emri İçin}
    
    LoopOrders -- Döngü Bitti --> CalcStats[İstatistikleri Hesapla: p50, p90, p99, p99.9]
    CalcStats --> SaveCSV[benchmark_results.csv Dosyasına Kaydet]
    SaveCSV --> SaveJSON[tail_latency.json Dosyasına Kaydet]
    SaveJSON --> End1([Tamamlandı])

    LoopOrders -- Sıradaki Emir --> SendTLS[TLS Proxy Port 5007 Soketine Bağlan & Gönder]
    SendTLS --> ReadTLS[TLS Yanıtını Al & Süreyi Hesapla]
    ReadTLS --> SendPQC[PQC Proxy Port 5006 Soketine Bağlan & Gönder]
    SendPQC --> ReadPQC[PQC Yanıtını Al & Süreyi Hesapla]
    ReadPQC --> LoopOrders
```

### B. Mikro Donanım Testi (micro_bench.cpp) Akışı:
```mermaid
flowchart TD
    Start2([Micro Bench Başlatıldı]) --> DetectArch{İşlemci Mimarisi Algılama}
    
    DetectArch -- x86_64 --> X86[RDTSC Donanım Talimatını Seç]
    DetectArch -- AArch64 / ARM64 --> ARM[cntvct_el0 Register Okuma Talimatını Seç]
    DetectArch -- Diğer --> Fallback[0 Dönen Yedek Metodu Seç]
    
    X86 & ARM & Fallback --> MeasureKEM[KEM Primitiflerini Test Et: X25519 vs ML-KEM]
    MeasureKEM --> MeasureDSA[DSA Primitiflerini Test Et: ECDSA vs ML-DSA]
    MeasureDSA --> SaveCycles[cpu_cycles.json Dosyasına Kaydet]
    SaveCycles --> End2([Tamamlandı])
```

---

## 4. Kullanılan Teknolojiler
Performans Test ve Benchmark bileşeninde kullanılan temel teknoloji ve standartlar şunlardır:

*   **C++17**: Gecikmelerin mikrosaniye düzeyinde doğru hesaplanması ve CPU döngülerinin donanım düzeyinde ölçülebilmesi için derlenebilir dil olarak kullanılmıştır.
*   **İşlemci Özel Düzey Kayıtçıları (Inlined Assembly)**:
    *   **x86_64 (`__rdtsc()`)**: x86 mimarilerinde işlemcinin sıfırdan itibaren saydığı saat vuruşu değerini (timestamp counter) çeken assembly talimatıdır.
    *   **AArch64 (`cntvct_el0`)**: ARM mimarilerinde (örn: Apple Silicon M1/M2/M3) sanal zaman sayacını çeken donanım yazmacı okuma talimatıdır.
*   **C++ std::sort & Yüzdelik CDF Algoritmaları**: Toplanan gecikme dizilerini sıralayıp kümülatif dağılım fonksiyonu (CDF) hesaplaması için standart kütüphane sıralama algoritmaları kullanılmıştır.
*   **POSIX TCP Sockets**: Test verilerini proksiler üzerinden Mock BIST'e gönderip almak için en düşük overhead'e sahip soket arayüzleri tercih edilmiştir.
*   **Python 3**: Random veri havuzları kullanarak FIX standartlarına uygun, tutarlı test emri dosyaları (`fix_test_data.txt`) üretmek için kullanılmıştır.
