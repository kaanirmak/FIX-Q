#include "finora/tcp_server.hpp"
#include "finora/logger.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <csignal>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static void bidirectional_forward(SSL* ssl, int ssl_fd, int plain_fd) {
    fd_set read_fds;
    char buffer[8192];
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(ssl_fd, &read_fds);
        FD_SET(plain_fd, &read_fds);
        int max_fd = std::max(ssl_fd, plain_fd);

        struct timeval tv;
        tv.tv_sec = 4;
        tv.tv_usec = 0;

        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret <= 0) break;

        if (FD_ISSET(plain_fd, &read_fds)) {
            ssize_t n = read(plain_fd, buffer, sizeof(buffer));
            if (n <= 0) break;
            if (SSL_write(ssl, buffer, n) <= 0) break;
        }
        if (FD_ISSET(ssl_fd, &read_fds)) {
            int n = SSL_read(ssl, buffer, sizeof(buffer));
            if (n <= 0) break;
            if (write(plain_fd, buffer, n) <= 0) break;
        }
    }
}

class TlsDecryptServer : public TcpServer {
private:
    SSL_CTX* server_ctx_;

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
        SSL* ssl = SSL_new(server_ctx_);
        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            SSL_free(ssl);
            close(client_fd);
            return;
        }

        int bist_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5003);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(bist_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            SSL_free(ssl);
            close(client_fd);
            return;
        }

        bidirectional_forward(ssl, client_fd, bist_fd);

        SSL_free(ssl);
        close(client_fd);
        close(bist_fd);
    }
};

class TlsClientEntryServer : public TcpServer {
private:
    SSL_CTX* client_ctx_;

public:
    TlsClientEntryServer(int port) : TcpServer(port, "TlsClientEntryServer"), client_ctx_(nullptr) {
        client_ctx_ = SSL_CTX_new(TLS_client_method());
        if (!client_ctx_) {
            throw std::runtime_error("TlsClientEntryServer: Failed to create client SSL context");
        }
        SSL_CTX_set_verify(client_ctx_, SSL_VERIFY_NONE, NULL);
    }

    ~TlsClientEntryServer() override {
        if (client_ctx_) {
            SSL_CTX_free(client_ctx_);
        }
    }

    void handle_client(int client_fd) override {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5008);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            close(client_fd);
            return;
        }

        SSL* ssl = SSL_new(client_ctx_);
        SSL_set_fd(ssl, server_fd);

        if (SSL_connect(ssl) <= 0) {
            SSL_free(ssl);
            close(server_fd);
            close(client_fd);
            return;
        }

        bidirectional_forward(ssl, server_fd, client_fd);

        SSL_free(ssl);
        close(client_fd);
        close(server_fd);
    }
};

int main() {
    signal(SIGPIPE, SIG_IGN);
    logger::log_info("Starting C++ TLS Proxy Suite...");

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    try {
        std::thread decrypt_thread([]() {
            try {
                TlsDecryptServer decrypt_server(5008);
                decrypt_server.run();
            } catch (const std::exception& e) {
                logger::log_error("TlsDecryptServer Fatal: " + std::string(e.what()));
            }
        });
        decrypt_thread.detach();

        TlsClientEntryServer entry_server(5007);
        entry_server.run();
    } catch (const std::exception& e) {
        logger::log_error("Fatal error starting TLS Proxy Suite: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
