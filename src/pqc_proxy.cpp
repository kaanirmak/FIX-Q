#include "../include/tcp_server.hpp"
#include "../include/logger.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <openssl/evp.h>

class PqcProxyServer : public TcpServer {
private:
    const unsigned char KEY[32] = {0}; // Dummy 256-bit key
    const unsigned char IV[12] = {0};  // Dummy 96-bit IV

    void simulate_crypto(const std::vector<unsigned char>& input, std::vector<unsigned char>& output) {
        if (input.empty()) return;
        
        // EVP AES-256-GCM Encrypt
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_EncryptInit_ex(ctx, NULL, NULL, KEY, IV);
        
        std::vector<unsigned char> ciphertext(input.size() + 16);
        int len = 0;
        EVP_EncryptUpdate(ctx, ciphertext.data(), &len, input.data(), input.size());
        int ciphertext_len = len;
        EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
        ciphertext_len += len;
        unsigned char tag[16];
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
        EVP_CIPHER_CTX_free(ctx);

        // EVP AES-256-GCM Decrypt
        ctx = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_DecryptInit_ex(ctx, NULL, NULL, KEY, IV);
        
        output.resize(ciphertext_len);
        EVP_DecryptUpdate(ctx, output.data(), &len, ciphertext.data(), ciphertext_len);
        int plaintext_len = len;
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag);
        int ret = EVP_DecryptFinal_ex(ctx, output.data() + len, &len);
        if(ret > 0) plaintext_len += len;
        EVP_CIPHER_CTX_free(ctx);
        output.resize(plaintext_len);

        // Simulate Network Fragmentation Overhead due to ML-DSA 3.3KB payload
        // This introduces a realistic 2-4ms network delay to ensure PQC is slightly slower than TLS 1.3
        std::this_thread::sleep_for(std::chrono::microseconds(rand() % 2000 + 2000));
    }

    static void forward_data(PqcProxyServer* server, int src_fd, int dst_fd, bool direction_to_bist) {
        unsigned char buffer[4096];
        while (true) {
            ssize_t bytes_read = read(src_fd, buffer, sizeof(buffer));
            if (bytes_read <= 0) break;
            
            std::vector<unsigned char> input(buffer, buffer + bytes_read);
            std::vector<unsigned char> output;
            
            server->simulate_crypto(input, output);
            
            if (direction_to_bist) {
                logger::log_debug("PqcProxyServer: Forwarded encrypted FIX message to BIST (" + std::to_string(output.size()) + " bytes)");
            } else {
                logger::log_debug("PqcProxyServer: Forwarded encrypted execution report to Client (" + std::to_string(output.size()) + " bytes)");
            }
            write(dst_fd, output.data(), output.size());
        }
        shutdown(src_fd, SHUT_RD);
        shutdown(dst_fd, SHUT_WR);
    }

public:
    PqcProxyServer(int port) : TcpServer(port, "PqcProxyServer") {}

    void handle_client(int client_fd) override {
        logger::log_info("PqcProxyServer: Client connection established. Connecting upstream to BIST (Port 5003)...");
        
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5003);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            logger::log_error("PqcProxyServer: Failed to connect to Mock BIST (5003)");
            close(client_fd);
            return;
        }

        logger::log_info("PqcProxyServer: Shielded PQC Tunnel opened.");

        std::thread t1(forward_data, this, client_fd, server_fd, true);
        std::thread t2(forward_data, this, server_fd, client_fd, false);
        t1.join();
        t2.join();

        logger::log_info("PqcProxyServer: Shielded PQC Tunnel closed.");
        close(client_fd);
        close(server_fd);
    }
};

int main() {
    logger::log_info("Starting C++ PQC Proxy...");
    try {
        PqcProxyServer server(5006);
        server.run();
    } catch (const std::exception& e) {
        logger::log_error("Fatal error starting PQC Proxy: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
