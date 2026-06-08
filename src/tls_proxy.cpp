#include "../include/tcp_server.hpp"
#include "../include/logger.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

class TlsDecryptServer : public TcpServer {
private:
    SSL_CTX* server_ctx_;

    static void proxy_tls_to_plain(SSL* ssl, int plain_fd) {
        char buffer[4096];
        while (true) {
            int bytes_read = SSL_read(ssl, buffer, sizeof(buffer));
            if (bytes_read <= 0) break;
            write(plain_fd, buffer, bytes_read);
        }
    }

    static void proxy_plain_to_tls(int plain_fd, SSL* ssl) {
        char buffer[4096];
        while (true) {
            ssize_t bytes_read = read(plain_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break;
            SSL_write(ssl, buffer, bytes_read);
        }
    }

public:
    TlsDecryptServer(int port) : TcpServer(port, "TlsDecryptServer"), server_ctx_(nullptr) {
        server_ctx_ = SSL_CTX_new(TLS_server_method());
        if (!server_ctx_) {
            throw std::runtime_error("TlsDecryptServer: Failed to create SSL context");
        }
        if (SSL_CTX_use_certificate_file(server_ctx_, "certs/server.crt", SSL_FILETYPE_PEM) <= 0) {
            throw std::runtime_error("TlsDecryptServer: Failed to load certs/server.crt");
        }
        if (SSL_CTX_use_PrivateKey_file(server_ctx_, "certs/server.key", SSL_FILETYPE_PEM) <= 0) {
            throw std::runtime_error("TlsDecryptServer: Failed to load certs/server.key");
        }
    }

    ~TlsDecryptServer() override {
        if (server_ctx_) {
            SSL_CTX_free(server_ctx_);
        }
    }

    void handle_client(int client_fd) override {
        logger::log_info("TlsDecryptServer: Accepted incoming TLS connection.");
        SSL* ssl = SSL_new(server_ctx_);
        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            logger::log_error("TlsDecryptServer: SSL handshake failed.");
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client_fd);
            return;
        }

        logger::log_info("TlsDecryptServer: SSL handshake successful. Connecting upstream to BIST (Port 5003)...");

        int bist_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5003);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(bist_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            logger::log_error("TlsDecryptServer: Failed upstream connection to BIST (5003).");
            SSL_free(ssl);
            close(client_fd);
            return;
        }

        std::thread t1(proxy_tls_to_plain, ssl, bist_fd);
        std::thread t2(proxy_plain_to_tls, bist_fd, ssl);
        t1.join();
        t2.join();

        logger::log_info("TlsDecryptServer: Closing client connections.");
        SSL_free(ssl);
        close(client_fd);
        close(bist_fd);
    }
};

class TlsClientEntryServer : public TcpServer {
private:
    SSL_CTX* client_ctx_;

    static void proxy_plain_to_tls(int plain_fd, SSL* ssl) {
        char buffer[4096];
        while (true) {
            ssize_t bytes_read = read(plain_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break;
            SSL_write(ssl, buffer, bytes_read);
        }
        shutdown(plain_fd, SHUT_RD);
    }

    static void proxy_tls_to_plain(SSL* ssl, int plain_fd) {
        char buffer[4096];
        while (true) {
            int bytes_read = SSL_read(ssl, buffer, sizeof(buffer));
            if (bytes_read <= 0) break;
            write(plain_fd, buffer, bytes_read);
        }
        shutdown(plain_fd, SHUT_WR);
    }

public:
    TlsClientEntryServer(int port) : TcpServer(port, "TlsClientEntryServer"), client_ctx_(nullptr) {
        client_ctx_ = SSL_CTX_new(TLS_client_method());
        if (!client_ctx_) {
            throw std::runtime_error("TlsClientEntryServer: Failed to create client SSL context");
        }
        SSL_CTX_set_verify(client_ctx_, SSL_VERIFY_NONE, NULL); // bypass verification for localhost self-signed cert
    }

    ~TlsClientEntryServer() override {
        if (client_ctx_) {
            SSL_CTX_free(client_ctx_);
        }
    }

    void handle_client(int client_fd) override {
        logger::log_info("TlsClientEntryServer: Accepted plaintext client connection. Establishing TLS tunnel to Port 5008...");

        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5008);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            logger::log_error("TlsClientEntryServer: Failed upstream connection to TLS Server (5008)");
            close(client_fd);
            return;
        }

        SSL* ssl = SSL_new(client_ctx_);
        SSL_set_fd(ssl, server_fd);

        if (SSL_connect(ssl) <= 0) {
            logger::log_error("TlsClientEntryServer: Upstream SSL handshake failed.");
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(server_fd);
            close(client_fd);
            return;
        }

        logger::log_info("TlsClientEntryServer: Upstream SSL handshake successful. Tunnel opened.");

        std::thread t1(proxy_plain_to_tls, client_fd, ssl);
        std::thread t2(proxy_tls_to_plain, ssl, client_fd);
        t1.join();
        t2.join();

        logger::log_info("TlsClientEntryServer: Tunnel closed.");
        SSL_free(ssl);
        close(client_fd);
        close(server_fd);
    }
};

int main() {
    logger::log_info("Starting C++ TLS Proxy Suite...");

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    try {
        // Start TlsDecryptServer on port 5008 in a background thread
        std::thread decrypt_thread([]() {
            try {
                TlsDecryptServer decrypt_server(5008);
                decrypt_server.run();
            } catch (const std::exception& e) {
                logger::log_error("TlsDecryptServer Fatal: " + std::string(e.what()));
            }
        });
        decrypt_thread.detach();

        // Start TlsClientEntryServer on port 5007 in the main thread loop
        TlsClientEntryServer entry_server(5007);
        entry_server.run();
    } catch (const std::exception& e) {
        logger::log_error("Fatal error starting TLS Proxy Suite: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
