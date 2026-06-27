# FIX-Q - C++ PQC Proxy ve Ağ Simülatörü Dökümanı

Bu döküman, **C++ PQC Proxy Server** bileşeninin detaylı analizini, iç mimarisini, akış süreçlerini ve kullanılan teknolojilerini açıklar.

---

## 1. Özet Paragrafı
**C++ PQC Proxy Server** (`src/pqc_proxy.cpp`), kuantum tehditlerine karşı geliştirilen koruma katmanının HFT ağlarındaki gerçekçi davranışını simüle eden, `Port 5006` üzerinde çalışan bir TCP tünel sunucusudur. İstemciden gelen bağlantıları kabul ettikten sonra Mock BIST sunucusuna (`Port 5003`) bir üst akış (upstream) bağlantısı kurar ve aradaki veri iletimini asenkron olarak yürütür. Proksinin en kritik görevi, kuantum sonrası şifreleme algoritmalarının (özellikle FIPS-204 standardı ML-DSA-65 imza şeması) ürettiği büyük boyutlu paketlerin (3.3 KB imza boyutu) Ethernet MTU sınırını aşması sonucu oluşan **ağ parçalanması (network fragmentation)** ve iletim gecikmesini simüle etmektir. Bunun için OpenSSL EVP API'si ile gerçekçi bir AES-256-GCM şifreleme/deşifreleme süreci işletir ve tünelden geçen her mesaja **2 ila 4 milisaniye** arasında rastgele seçilen mikro saniye hassasiyetli bir ağ gecikmesi enjekte eder.

---

## 2. Mimari Şeması
Aşağıdaki şema, PQC Proxy Server'ın bileşen seviyesindeki mimarisini ve BIST simülatörüyle olan ilişkisini gösterir:

```mermaid
graph TD
    Client([HFT Client / Web UI]) -->|1. TCP Port 5006| PqcProxy[PqcProxyServer Sınıfı]
    
    subgraph PqcProxyServer
        direction TB
        PqcProxy -->|2. Client Handler| HandleClient[handle_client]
        HandleClient -->|3. Connect Upstream| BistSocket[Socket to BIST Port 5003]
        
        HandleClient -->|4. Start Forward Threads| Thread1[Thread 1: Forward Client to BIST]
        HandleClient -->|4. Start Forward Threads| Thread2[Thread 2: Forward BIST to Client]
        
        subgraph SimulatePqcArmor ["Simüle PQC Zırhı ve Gecikme"]
            Thread1 -->|5. Encrypt / Decrypt| EVP_AES[OpenSSL EVP AES-256-GCM]
            EVP_AES -->|6. Enjekte Edilen Gecikme| Delay[std::this_thread::sleep_for 2-4ms]
        end
    end

    Delay -->|7. Forwarded FIX payload| MockBist[Mock BIST Server - Port 5003]
```

---

## 3. Akış Şeması
PQC Proxy Server üzerinden verinin akış ve gecikme simülasyonu adımları aşağıdaki gibidir:

```mermaid
flowchart TD
    Start([PQC Proxy Sunucusu Başlatıldı - Port 5006]) --> AcceptConn[İstemci Bağlantısını Kabul Et]
    AcceptConn --> ConnectUpstream[Mock BIST Sunucusuna Bağlan - Port 5003]
    ConnectUpstream --> OpenTunnel[Çift Yönlü PQC Tüneli Açıldı]
    
    OpenTunnel --> StartThreads[İki Yönlü Yönlendirme İş Parçacıklarını Başlat]
    
    subgraph ForwardLoop ["Yönlendirme Döngüsü (forward_data)"]
        StartThreads --> Read[Soketten Veri Oku]
        Read --> CheckData{Veri Var mı?}
        CheckData -- Hayır/Bağlantı Kapandı --> Shutdown[Soketleri Kapat & Tüneli Kapat]
        CheckData -- Evet --> CallSimulate[simulate_crypto Metodunu Çağır]
        
        subgraph simulate_crypto
            CallSimulate --> InitAES[EVP_EncryptInit: AES-256-GCM Şifreleme]
            InitAES --> AESEncrypt[EVP_EncryptUpdate & EVP_EncryptFinal]
            AESEncrypt --> InitDecrypt[EVP_DecryptInit: AES-256-GCM Deşifreleme]
            InitDecrypt --> AESDecrypt[EVP_DecryptUpdate & EVP_DecryptFinal]
            AESDecrypt --> CalcDelay[Ağ Parçalanması ve ML-DSA İmza Boyutu İçin 2-4ms Gecikme Hesapla]
            CalcDelay --> Sleep[std::this_thread::sleep_for Gecikmeyi Uygula]
        end
        
        Sleep --> Write[Hedef Sokete Düz Metin / İşlenmiş Veriyi Yaz]
        Write --> Read
    end
```

---

## 4. Kullanılan Teknolojiler
C++ PQC Proxy Server bileşeninde kullanılan temel teknoloji ve standartlar şunlardır:

*   **C++17**: POSIX soket yönetimi ve nesne yönelimli TCP sunucu yapısı için ana dil olarak kullanılmıştır.
*   **OpenSSL EVP API**: AES-256-GCM gibi yüksek güvenlikli simetrik şifreleme algoritmalarını donanım ivmeli (AES-NI) ve güvenli bir şekilde çalıştırmak amacıyla kullanılmıştır.
*   **C++ std::chrono & std::this_thread**: Simüle edilen kuantum sonrası imza paket gecikmelerini (2-4 ms aralığında mikro saniye hassasiyetli gecikmeler) enjekte etmek amacıyla kullanılmıştır.
*   **POSIX Sockets**: Yüksek performanslı TCP veri iletimi ve çift yönlü soket kapatma işlemleri (`shutdown(fd, SHUT_RD/WR)`) için kullanılmıştır.
*   **FIPS 203/204 Boyut Modellemesi**: ML-KEM ve ML-DSA algoritmalarının paket büyüklükleri ve ağ üzerindeki olası TCP paket bölünmesi etkileri gecikme parametrelerinin temelini oluşturmaktadır.
