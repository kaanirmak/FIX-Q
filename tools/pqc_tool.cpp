#include "finora/pqc_crypto.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <openssl/rand.h>
#include <openssl/evp.h>

std::string hex_to_bytes_str(const std::string& hex) {
    std::string bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string clean_fix(const std::string& fix) {
    std::string out = fix;
    for (char& c : out) {
        if (c == '\x01') c = '|';
    }
    return out;
}

int main(int argc, char* argv[]) {
    std::string hex_message = "";
    std::string bist_response_hex = "";
    double tls_latency = 0.0;
    double pqc_latency = 0.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--hex-message" && i + 1 < argc) {
            hex_message = argv[++i];
        } else if (arg == "--bist-response-hex" && i + 1 < argc) {
            bist_response_hex = argv[++i];
        } else if (arg == "--tls-latency" && i + 1 < argc) {
            tls_latency = std::stod(argv[++i]);
        } else if (arg == "--pqc-latency" && i + 1 < argc) {
            pqc_latency = std::stod(argv[++i]);
        }
    }

    if (hex_message.empty()) {
        std::cerr << "{\"status\":\"error\",\"error_message\":\"Missing --hex-message\"}\n";
        return 1;
    }

    try {
        std::string raw_fix = hex_to_bytes_str(hex_message);
        std::string bist_resp = "";
        if (!bist_response_hex.empty()) {
            bist_resp = clean_fix(hex_to_bytes_str(bist_response_hex));
        }

        // 1. Genuine Finora PQC Wrapping via OpenSSL 3.6.2 (Spec v1.4.0-DEV)
        PqcEngine engine;
        PqcWireBreakdown breakdown;
        std::vector<unsigned char> wire_packet = engine.wrap_packet(raw_fix, &breakdown);

        // 2. Standard TLS 1.3 Record Construction
        unsigned char tls_key[32];
        unsigned char tls_nonce[12];
        RAND_bytes(tls_key, 32);
        RAND_bytes(tls_nonce, 12);

        std::string inner_plain = raw_fix + "\x17";
        EVP_CIPHER_CTX* t_ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(t_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_EncryptInit_ex(t_ctx, NULL, NULL, tls_key, tls_nonce);

        std::vector<unsigned char> tls_cipher(inner_plain.size() + 16);
        int tlen1 = 0, tlen2 = 0;
        EVP_EncryptUpdate(t_ctx, tls_cipher.data(), &tlen1, (const unsigned char*)inner_plain.data(), inner_plain.size());
        EVP_EncryptFinal_ex(t_ctx, tls_cipher.data() + tlen1, &tlen2);
        unsigned char tls_tag[16];
        EVP_CIPHER_CTX_ctrl(t_ctx, EVP_CTRL_GCM_GET_TAG, 16, tls_tag);
        EVP_CIPHER_CTX_free(t_ctx);
        tls_cipher.resize(tlen1 + tlen2);

        uint16_t tls_rec_len = (uint16_t)(tls_cipher.size() + 16);
        unsigned char tls_hdr[5] = {0x17, 0x03, 0x03, (unsigned char)((tls_rec_len >> 8) & 0xFF), (unsigned char)(tls_rec_len & 0xFF)};
        size_t tls_total_size = 5 + tls_cipher.size() + 16;

        // Output JSON conforming to Finora Framework Technical Specification
        std::cout << "{\n";
        std::cout << "  \"status\": \"success\",\n";
        std::cout << "  \"plaintext\": \"" << clean_fix(raw_fix) << "\",\n";
        std::cout << "  \"bist_response\": \"" << bist_resp << "\",\n";
        std::cout << "  \"tls_latency\": " << tls_latency << ",\n";
        std::cout << "  \"pqc_latency\": " << pqc_latency << ",\n";
        std::cout << "  \"finora_magic_hex\": \"464e\",\n";
        std::cout << "  \"finora_version\": 1,\n";
        std::cout << "  \"finora_payload_type\": 16,\n";
        std::cout << "  \"session_id\": " << breakdown.session_id << ",\n";
        std::cout << "  \"sequence_num\": " << breakdown.sequence_num << ",\n";
        std::cout << "  \"pqc_total_wire_size\": " << breakdown.total_wire_size << ",\n";
        std::cout << "  \"pqc_plaintext_size\": " << breakdown.plaintext_size << ",\n";
        std::cout << "  \"x25519_pubkey_hex\": \"" << breakdown.x25519_pubkey_hex << "\",\n";
        std::cout << "  \"kem_ciphertext_hex\": \"" << breakdown.kem_ciphertext_hex << "\",\n";
        std::cout << "  \"aes_nonce_hex\": \"" << breakdown.aes_nonce_hex << "\",\n";
        std::cout << "  \"aes_ciphertext_hex\": \"" << breakdown.aes_ciphertext_hex << "\",\n";
        std::cout << "  \"aes_tag_hex\": \"" << breakdown.aes_tag_hex << "\",\n";
        std::cout << "  \"mldsa_signature_hex\": \"" << breakdown.mldsa_signature_hex << "\",\n";
        std::cout << "  \"tls_total_wire_size\": " << tls_total_size << ",\n";
        std::cout << "  \"tls_header_hex\": \"" << bytes_to_hex(tls_hdr, 5) << "\",\n";
        std::cout << "  \"tls_nonce_hex\": \"" << bytes_to_hex(tls_nonce, 12) << "\",\n";
        std::cout << "  \"tls_ciphertext_hex\": \"" << bytes_to_hex(tls_cipher) << "\",\n";
        std::cout << "  \"tls_tag_hex\": \"" << bytes_to_hex(tls_tag, 16) << "\"\n";
        std::cout << "}\n";

    } catch (const std::exception& e) {
        std::cout << "{\"status\":\"error\",\"error_message\":\"" << e.what() << "\"}\n";
        return 1;
    }

    return 0;
}
