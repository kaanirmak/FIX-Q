# FIX-Q - TLS 1.3 Proxy Tüneli Dökümanı

Bu döküman, **TLS 1.3 Proxy Suite** bileşeninin detaylı analizini, iç mimarisini, akış süreçlerini ve kullanılan teknolojilerini açıklar.

---

## 1. Özet Paragrafı
**TLS 1.3 Proxy Suite** (`src/tls_proxy.cpp`), klasik finansal altyapılarda kullanılan güvenli haberleşme standardı **TLS 1.3** tünellemesini gerçekleştiren, yüksek performanslı ve çift taraflı çalışan bir C++ sunucu uygulamasıdır. Tek bir çalıştırılabilir dosya içinde iki farklı TCP sunucusu barındırır:
1.  **TlsClientEntryServer** (`Port 5007`): İstemcilerden gelen yerel düz metin (plaintext) FIX bağlantılarını kabul eder ve bunları OpenSSL istemcisi olarak TLS el sıkışmasıyla port 5008'deki sunucuya tüneller.
2.  **TlsDecryptServer** (`Port 5008`): Port 5007'den gelen TLS şifreli verileri yakalar, `certs/server.crt` ve `certs/server.key` dosyaları yardımıyla deşifre eder (decrypt) ve düz metin FIX verisini doğrudan Mock BIST sunucusuna (`Port 5003`) iletir.

Bu yapı sayesinde, normalde TLS desteklemeyen düz metin HFT istemcilerinin ağ üzerinden TLS 1.3 ile şifrelenmiş olarak haberleşmesi ve hedefe ulaşmadan önce şifrelerinin güvenle çözülmesi simüle edilmiş olur.

---

## 2. Mimari Şeması
Aşağıdaki şema, TLS Proxy Suite'in çift taraflı tünelleme mimarisini ve veri akış yönünü göstermektedir:

```mermaid
graph TD
    Client([HFT Client - Plaintext]) -->|1. TCP Port 5007| EntryServer[TlsClientEntryServer]
    
    subgraph TLSProxySuite ["TLS Proxy Suite"]
        direction TB
        subgraph Port5007Entry ["Port 5007 - İstemci Girişi"]
            EntryServer -->|2. SSL_connect| TLSClient[OpenSSL Client Context]
        end
        
        TLSClient -->|3. TLS 1.3 Encrypted Tunnel| DecryptServer[TlsDecryptServer - Port 5008]
        
        subgraph Port5008Decrypt ["Port 5008 - Şifre Çözücü Sunucu"]
            DecryptServer -->|4. Load Certs & SSL_accept| SSLServerContext[OpenSSL Server Context]
        end
    end

    SSLServerContext -->|5. Plaintext Upstream| MockBist[Mock BIST Server - Port 5003]
```

---

## 3. Akış Şeması
TLS Proxy Suite'in başlatılması ve bir emrin çift yönlü aktarılma süreci aşağıdaki akış şemasında gösterilmiştir:

```mermaid
flowchart TD
    Start([TLS Proxy Suite Başlatıldı]) --> InitOpenSSL[OpenSSL Kütüphanesini Başlat ve Algoritmaları Yükle]
    InitOpenSSL --> StartDecrypt[Thread 1: TlsDecryptServer Başlat - Port 5008]
    InitOpenSSL --> StartEntry[Thread 2 / Main: TlsClientEntryServer Başlat - Port 5007]
    
    subgraph TlsDecryptFlow ["TlsDecryptServer Sunucu Akışı (Port 5008)"]
        StartDecrypt --> LoadCerts[certs/server.crt ve server.key Yükle]
        LoadCerts --> AcceptTLS[Bağlantı Kabul Et & SSL_accept ile El Sıkışması Yap]
        AcceptTLS --> ConnectBist[Mock BIST Sunucusuna Bağlan - Port 5003]
        ConnectBist --> ProxyThreads[İki Yönlü Veri Aktarımı İçin Thread Başlat]
        ProxyThreads -->|Thread A| DecryptData[SSL_read -> write plaintext to BIST]
        ProxyThreads -->|Thread B| EncryptResp[read plaintext from BIST -> SSL_write]
    end

    subgraph TlsClientFlow ["TlsClientEntryServer İstemci Akışı (Port 5007)"]
        StartEntry --> AcceptPlain[Düz Metin İstemci Bağlantısı Kabul Et]
        AcceptPlain --> ConnectDecrypt[TlsDecryptServer Sunucusuna Bağlan - Port 5008]
        ConnectDecrypt --> Handshake[SSL_new & SSL_connect ile TLS El Sıkışması Gerçekleştir]
        Handshake --> TunnelThreads[İki Yönlü Veri Aktarımı İçin Thread Başlat]
        TunnelThreads -->|Thread C| PlainToTLS[read plaintext from Client -> SSL_write]
        TunnelThreads -->|Thread D| TLSToPlain[SSL_read -> write plaintext to Client]
    end
```

---

## 4. Kullanılan Teknolojiler
TLS 1.3 Proxy Suite bileşeninde kullanılan temel teknoloji ve kütüphaneler şunlardır:

*   **C++17 ve POSIX Sockets**: Sunucunun ağ iletişimi ve yüksek performanslı tampon yönetimi için kullanılmıştır.
*   **OpenSSL (libssl & libcrypto)**: Klasik şifreleme, TLS el sıkışma yönetimi, sertifika doğrulama (veya bu senaryoda localhost bypass) ve TLS 1.3 kaydı şifreleme/deşifreleme işlemleri için ana kütüphanedir.
*   **PEM Sertifikaları**: Sunucu kimlik doğrulaması için `certs/server.crt` (X.509 formatında sertifika) ve `certs/server.key` (RSA/ECC Özel Anahtarı) kullanılmıştır.
*   **C++ std::thread**: Hem iki farklı sunucuyu paralel çalıştırmak hem de her bağlantı için soketler arası veri kopyalama işlemlerini (`proxy_plain_to_tls` ve `proxy_tls_to_plain`) asenkron olarak yürütmek için kullanılmıştır.
*   **TCP_NODELAY**: Paketlerin ağda birikmesini önlemek ve TLS tünelindeki HFT gecikmelerini en aza indirmek için soket seviyesinde ayarlanmıştır.
