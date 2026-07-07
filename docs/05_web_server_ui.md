# FIX-Q - Web Sunucusu ve Dashboard Arayüzü Dökümanı

Bu döküman, **C++ Web Server** ve **HTML5/JS Dashboard Arayüzü** bileşenlerinin detaylı analizini, iç mimarisini, akış süreçlerini ve kullanılan teknolojilerini açıklar.

---

## 1. Özet Paragrafı
**Web Sunucusu ve Dashboard Arayüzü**, FIX-Q platformunun yönetim merkezini ve kullanıcı etkileşim katmanını oluşturur. Backend, tek bir header'dan oluşan hafif `httplib.h` kütüphanesi üzerine kurulu bir C++ web sunucusudur ve `Port 8080` üzerinde çalışır. Statik web dosyalarını (`ui/static/index.html`) doğrudan istemciye sunmanın yanı sıra; proksilerden veri toplayan, toplu testleri başlatan ve kriptografik analiz komutlarını (`encrypt_order.py`) tetikleyen REST API uç noktaları (endpoints) barındırır. Tarayıcı tarafında çalışan arayüz ise; modern **Outfit** ve **JetBrains Mono** fontları ile tasarlanmış, tek sayfalık (SPA) premium bir terminal arayüzüdür. Kullanıcılar bu arayüz üzerinden manuel emirler gönderebilir, bu emirlerin TLS ve PQC proksileri üzerindeki anlık gecikmelerini ölçebilir, kablo üzerindeki (on-the-wire) paket yapısının bayt bayt görsel dökümünü inceleyebilir ve Klasik Bilgisayar (Normal PC) ile Kuantum Bilgisayarı (Quantum PC) kırma sürelerini ve kuantum güvenlik risklerini kıyaslayabilirler.

---

## 2. Mimari Şeması
Aşağıdaki şema, Web Sunucusu ile diğer sistem bileşenleri ve istemci tarayıcısı arasındaki veri yollarını göstermektedir:

```mermaid
graph TD
    User([Kullanıcı Tarayıcısı]) -->|1. HTTP / API Request| WebServer[C++ Web Server - Port 8080]
    
    subgraph WebServerEndpoints ["Web Server API Endpoints"]
        WebServer -->|GET /| MountPoint[Static Files: index.html]
        WebServer -->|GET /api/benchmark_data| ParseCSV[benchmark_results.csv Okuyucu]
        WebServer -->|GET /api/academic_data| ParseJSON[tail_latency.json & cpu_cycles.json Okuyucu]
        WebServer -->|POST /api/upload_orders| UploadHandler[Toplu Veri Yükleyici & benchmark çalıştırıcı]
        WebServer -->|POST /api/send_single_order| OrderHandler[Tekil Emir Yöneticisi]
    end

    OrderHandler -->|2. Measure Latency| TLSProxy[TLS Proxy Entry - Port 5007]
    OrderHandler -->|2. Measure Latency| PQCProxy[PQC Proxy Entry - Port 5006]
    OrderHandler -->|3. Call crypto analyzer| PyScript[python3 crypto/encrypt_order.py]
    
    UploadHandler -->|4. Run benchmark binary| BenchBin[./bin/benchmark]
```

---

## 3. Akış Şeması
Aşağıdaki akış şeması, kullanıcının tek bir emri manuel gönderdiğinde arka planda çalışan API akışını göstermektedir:

```mermaid
flowchart TD
    Start([Kullanıcı Arayüzden Emir Gönderdi]) --> PostAPI[POST /api/send_single_order]
    PostAPI --> ExtractFields[JSON verisini ayrıştır: symbol, side, qty, price]
    ExtractFields --> CheckFields{Alanlar Eksik mi?}
    CheckFields -- Evet --> ErrorResp[Hata JSON'u dön]
    CheckFields -- Hayır --> BuildFIX[FIX NewOrderSingle iletisi oluştur - 35=D]
    
    BuildFIX --> MeasureTLS[TLS Proksisine gönder Port 5007 ve süreyi ölç]
    BuildFIX --> MeasurePQC[PQC Proksisine gönder Port 5006 ve süreyi ölç]
    
    MeasureTLS & MeasurePQC --> CheckFallback{Bağlantı koptu/kapalı mı?}
    CheckFallback -- Evet --> SimFallback[Matematiksel Simülasyon Gecikmesi Enjekte Et]
    CheckFallback -- Hayır --> ReadResp[BIST yanıtını oku ExecutionReport]
    
    SimFallback & ReadResp --> RunPy[Python Scripti Çalıştır: python3 encrypt_order.py]
    RunPy --> PyOutput[Kripto paket ayrışım verilerini JSON olarak al]
    PyOutput --> RespSuccess[Yanıtı JSON formatında istemciye dön]
    RespSuccess --> RenderUI[Arayüzde Paket Çubuğu, Gecikme Kartları ve Hex Dökümünü Güncelle]
```

---

## 4. Kullanılan Teknolojiler
Web Sunucusu ve Dashboard Arayüzü bileşeninde kullanılan temel teknoloji ve standartlar şunlardır:

*   **C++17 ve httplib.h**: Sıfır bağımlılıklı, yüksek performanslı ve multithreaded HTTP API backend sunucusu oluşturmak için kullanılmıştır.
*   **HTML5 & CSS3**: Glassmorphism (arka plan bulanıklığı), CSS değişkenleri yardımıyla dinamik renk paletleri ve akıcı geçiş animasyonları içeren modern bir dashboard tasarımı için kullanılmıştır.
*   **JavaScript (ES6+)**: Sayfa yenilenmeden asenkron API istekleri (fetch API), dosya yükleme yönetimi (drag and drop) ve DOM manipülasyonları gerçekleştirmek için kullanılmıştır.
*   **Chart.js**: Gecikme grafiklerini, CDF eğrilerini ve CPU döngü kıyaslamalarını dinamik ve etkileşimli olarak çizdirmek için tarayıcı tarafında kullanılmıştır.
*   **popen & Process Piping**: Web sunucusunun işletim sistemi seviyesinde Python şifreleme betiğini ve C++ benchmark uygulamalarını alt süreç (sub-process) olarak çalıştırıp çıktılarını yakalaması için kullanılmıştır.
