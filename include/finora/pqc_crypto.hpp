#pragma once

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <arpa/inet.h>

// Finora Framework Technical Specification v1.4.0-DEV Wire Format Constants
namespace finora {
    constexpr uint16_t MAGIC = 0x464E; // "FN"
    constexpr uint8_t  VERSION = 0x01;
    
    // Payload Types
    constexpr uint8_t  TYPE_HANDSHAKE_CLIENT_HELLO = 0x01;
    constexpr uint8_t  TYPE_HANDSHAKE_SERVER_HELLO = 0x02;
    constexpr uint8_t  TYPE_FIX_TRAFFIC            = 0x10;
    constexpr uint8_t  TYPE_OUCH_FRAME             = 0x11;
    constexpr uint8_t  TYPE_ISO20022_SWIFT         = 0x20;
    constexpr uint8_t  TYPE_WEB3_RPC               = 0x30;

    // Cryptographic Primitive Sizes (FIPS 203 & 204)
    constexpr size_t X25519_PUBKEY_SIZE        = 32;
    constexpr size_t MLKEM768_CIPHERTEXT_SIZE   = 1088;
    constexpr size_t MLKEM768_SHARED_SECRET_SIZE= 32;
    constexpr size_t AES_GCM_NONCE_SIZE         = 12;
    constexpr size_t AES_GCM_TAG_SIZE           = 16;
    constexpr size_t MLDSA65_SIGNATURE_SIZE     = 3309;
    constexpr size_t MLDSA65_PUBKEY_SIZE        = 1952;

#pragma pack(push, 1)
    struct WireHeader {
        uint16_t magic;         // 0x464E
        uint8_t  version;       // 0x01
        uint8_t  payload_type;  // 0x10
        uint32_t session_id;    // Session ID
        uint64_t sequence_num;  // Monotonic sequence counter
        uint32_t payload_len;   // Length of encrypted payload
        uint8_t  iv[12];        // 96-bit IV
    };
#pragma pack(pop)
}

struct PqcWireBreakdown {
    uint16_t magic = finora::MAGIC;
    uint8_t  version = finora::VERSION;
    uint8_t  payload_type = finora::TYPE_FIX_TRAFFIC;
    uint32_t session_id = 1;
    uint64_t sequence_num = 1;
    std::string x25519_pubkey_hex;
    std::string kem_ciphertext_hex;
    std::string aes_nonce_hex;
    std::string aes_ciphertext_hex;
    std::string aes_tag_hex;
    std::string mldsa_signature_hex;
    size_t total_wire_size = 0;
    size_t plaintext_size = 0;
};

inline std::string bytes_to_hex(const unsigned char* data, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        hex.push_back(hex_chars[(data[i] >> 4) & 0x0F]);
        hex.push_back(hex_chars[data[i] & 0x0F]);
    }
    return hex;
}

inline std::string bytes_to_hex(const std::vector<unsigned char>& data) {
    return bytes_to_hex(data.data(), data.size());
}

class PqcEngine {
private:
    EVP_PKEY* server_kem_key_ = nullptr;     // ML-KEM-768
    EVP_PKEY* client_dsa_key_ = nullptr;     // ML-DSA-65
    EVP_PKEY* static_peer_x25519_ = nullptr; // X25519
    std::atomic<uint64_t> sequence_counter_{1000};
    uint32_t  session_id_ = 0x00010001;
    uint32_t  salt_prefix_ = 0x5F4E5101;     // "FNQ\x01"

    uint8_t   cached_session_key_[32]{};
    bool      has_cached_session_key_{false};

    void init_crypto_primitives() {
        // 1. Generate NIST FIPS 203 ML-KEM-768 Keypair
        EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL);
        if (!kctx || EVP_PKEY_keygen_init(kctx) <= 0 || EVP_PKEY_generate(kctx, &server_kem_key_) <= 0) {
            if (kctx) EVP_PKEY_CTX_free(kctx);
            throw std::runtime_error("PqcEngine: Failed to generate OpenSSL ML-KEM-768 key");
        }
        EVP_PKEY_CTX_free(kctx);

        // 2. Generate NIST FIPS 204 ML-DSA-65 Keypair
        EVP_PKEY_CTX* sctx = EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-65", NULL);
        if (!sctx || EVP_PKEY_keygen_init(sctx) <= 0 || EVP_PKEY_generate(sctx, &client_dsa_key_) <= 0) {
            if (sctx) EVP_PKEY_CTX_free(sctx);
            throw std::runtime_error("PqcEngine: Failed to generate OpenSSL ML-DSA-65 key");
        }
        EVP_PKEY_CTX_free(sctx);

        // 3. Generate Static Peer X25519 Keypair for Gateway
        EVP_PKEY_CTX* xctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
        if (!xctx || EVP_PKEY_keygen_init(xctx) <= 0 || EVP_PKEY_generate(xctx, &static_peer_x25519_) <= 0) {
            if (xctx) EVP_PKEY_CTX_free(xctx);
            throw std::runtime_error("PqcEngine: Failed to generate OpenSSL X25519 key");
        }
        EVP_PKEY_CTX_free(xctx);

        // Establish initial hybrid session
        establish_session();
    }

public:
    PqcEngine() {
        init_crypto_primitives();
    }

    ~PqcEngine() {
        if (server_kem_key_) EVP_PKEY_free(server_kem_key_);
        if (client_dsa_key_) EVP_PKEY_free(client_dsa_key_);
        if (static_peer_x25519_) EVP_PKEY_free(static_peer_x25519_);
    }

    // Establishes hybrid session key via ML-KEM-768 + X25519 (§3.3 Phase 1)
    void establish_session(const uint8_t* salt = nullptr, size_t salt_len = 0) {
        using namespace finora;
        EVP_PKEY_CTX* xctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
        EVP_PKEY_keygen_init(xctx);
        EVP_PKEY* client_x = nullptr;
        EVP_PKEY_generate(xctx, &client_x);
        EVP_PKEY_CTX_free(xctx);

        EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(client_x, NULL);
        EVP_PKEY_derive_init(derive_ctx);
        EVP_PKEY_derive_set_peer(derive_ctx, static_peer_x25519_);
        size_t x_ss_len = 0;
        EVP_PKEY_derive(derive_ctx, NULL, &x_ss_len);
        std::vector<unsigned char> x_ss(x_ss_len);
        EVP_PKEY_derive(derive_ctx, x_ss.data(), &x_ss_len);
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(client_x);

        EVP_PKEY_CTX* enc_ctx = EVP_PKEY_CTX_new_from_pkey(NULL, server_kem_key_, NULL);
        EVP_PKEY_encapsulate_init(enc_ctx, NULL);
        size_t kem_ct_len = MLKEM768_CIPHERTEXT_SIZE;
        size_t kem_ss_len = MLKEM768_SHARED_SECRET_SIZE;
        std::vector<unsigned char> kem_ct(kem_ct_len);
        std::vector<unsigned char> kem_ss(kem_ss_len);
        EVP_PKEY_encapsulate(enc_ctx, kem_ct.data(), &kem_ct_len, kem_ss.data(), &kem_ss_len);
        EVP_PKEY_CTX_free(enc_ctx);

        std::vector<unsigned char> combined_ss = x_ss;
        combined_ss.insert(combined_ss.end(), kem_ss.begin(), kem_ss.end());

        EVP_KDF* kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
        EVP_KDF_CTX* kdf_ctx = EVP_KDF_CTX_new(kdf);
        EVP_KDF_free(kdf);
        OSSL_PARAM params[5];
        char md[] = "SHA256";
        params[0] = OSSL_PARAM_construct_utf8_string("digest", md, 0);
        params[1] = OSSL_PARAM_construct_octet_string("key", combined_ss.data(), combined_ss.size());
        params[2] = OSSL_PARAM_construct_octet_string("info", (void*)"Finora-v1-Traffic", 17);
        params[3] = OSSL_PARAM_construct_octet_string("salt", (void*)(salt ? (const char*)salt : "Finora-PQC-Salt-2026"), salt ? salt_len : 20);
        params[4] = OSSL_PARAM_construct_end();

        EVP_KDF_derive(kdf_ctx, cached_session_key_, 32, params);
        EVP_KDF_CTX_free(kdf_ctx);
        has_cached_session_key_ = true;
    }

    // Encrypts FIX payload into Finora Binary Wire Format with Hybrid PQC Shield
    std::vector<unsigned char> wrap_packet(const std::string& plaintext, PqcWireBreakdown* breakdown = nullptr) {
        using namespace finora;

        uint64_t seq = ++sequence_counter_;

        // 1. Generate Ephemeral X25519 Keypair
        EVP_PKEY_CTX* xctx = EVP_PKEY_CTX_new_from_name(NULL, "X25519", NULL);
        EVP_PKEY_keygen_init(xctx);
        EVP_PKEY* client_x = nullptr;
        EVP_PKEY_generate(xctx, &client_x);
        EVP_PKEY_CTX_free(xctx);

        size_t x_pub_len = X25519_PUBKEY_SIZE;
        std::vector<unsigned char> x25519_pub(x_pub_len);
        EVP_PKEY_get_raw_public_key(client_x, x25519_pub.data(), &x_pub_len);

        // Compute Classical X25519 Shared Secret
        EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(client_x, NULL);
        EVP_PKEY_derive_init(derive_ctx);
        EVP_PKEY_derive_set_peer(derive_ctx, static_peer_x25519_);
        size_t x_ss_len = 0;
        EVP_PKEY_derive(derive_ctx, NULL, &x_ss_len);
        std::vector<unsigned char> x_ss(x_ss_len);
        EVP_PKEY_derive(derive_ctx, x_ss.data(), &x_ss_len);
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(client_x);

        // 2. Encapsulate NIST FIPS 203 ML-KEM-768
        EVP_PKEY_CTX* enc_ctx = EVP_PKEY_CTX_new_from_pkey(NULL, server_kem_key_, NULL);
        EVP_PKEY_encapsulate_init(enc_ctx, NULL);
        size_t kem_ct_len = MLKEM768_CIPHERTEXT_SIZE;
        size_t kem_ss_len = MLKEM768_SHARED_SECRET_SIZE;
        std::vector<unsigned char> kem_ct(kem_ct_len);
        std::vector<unsigned char> kem_ss(kem_ss_len);
        EVP_PKEY_encapsulate(enc_ctx, kem_ct.data(), &kem_ct_len, kem_ss.data(), &kem_ss_len);
        EVP_PKEY_CTX_free(enc_ctx);

        // 3. Hybrid KDF according to Section 3.3:
        // SS_hybrid = SS_classic || SS_pqc
        // HKDF-Extract(Salt, SS_hybrid) -> HKDF-Expand(K_session, "Finora-v1-Traffic")
        std::vector<unsigned char> combined_ss = x_ss;
        combined_ss.insert(combined_ss.end(), kem_ss.begin(), kem_ss.end());

        EVP_KDF* kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
        EVP_KDF_CTX* kdf_ctx = EVP_KDF_CTX_new(kdf);
        EVP_KDF_free(kdf);
        OSSL_PARAM params[5];
        char md[] = "SHA256";
        params[0] = OSSL_PARAM_construct_utf8_string("digest", md, 0);
        params[1] = OSSL_PARAM_construct_octet_string("key", combined_ss.data(), combined_ss.size());
        params[2] = OSSL_PARAM_construct_octet_string("info", (void*)"Finora-v1-Traffic", 17);
        params[3] = OSSL_PARAM_construct_octet_string("salt", (void*)"Finora-PQC-Salt-2026", 20);
        params[4] = OSSL_PARAM_construct_end();

        std::vector<unsigned char> aes_key(32);
        EVP_KDF_derive(kdf_ctx, aes_key.data(), 32, params);
        EVP_KDF_CTX_free(kdf_ctx);

        // 4. Construct 96-bit Determinate IV (64-bit Sequence Counter + 32-bit Salt)
        uint8_t iv[AES_GCM_NONCE_SIZE];
        for (int i = 0; i < 8; ++i) {
            iv[i] = (uint8_t)((seq >> (56 - i * 8)) & 0xFF);
        }
        iv[8]  = (uint8_t)((salt_prefix_ >> 24) & 0xFF);
        iv[9]  = (uint8_t)((salt_prefix_ >> 16) & 0xFF);
        iv[10] = (uint8_t)((salt_prefix_ >> 8) & 0xFF);
        iv[11] = (uint8_t)(salt_prefix_ & 0xFF);

        // 5. Encrypt Financial Payload with AES-256-GCM
        EVP_CIPHER_CTX* c_ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(c_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_EncryptInit_ex(c_ctx, NULL, NULL, aes_key.data(), iv);

        std::vector<unsigned char> ciphertext(plaintext.size() + 16);
        int len1 = 0, len2 = 0;
        EVP_EncryptUpdate(c_ctx, ciphertext.data(), &len1, (const unsigned char*)plaintext.data(), plaintext.size());
        EVP_EncryptFinal_ex(c_ctx, ciphertext.data() + len1, &len2);
        unsigned char tag[AES_GCM_TAG_SIZE];
        EVP_CIPHER_CTX_ctrl(c_ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, tag);
        EVP_CIPHER_CTX_free(c_ctx);
        ciphertext.resize(len1 + len2);

        // 6. Sign Envelope with FIPS 204 ML-DSA-65
        std::vector<unsigned char> to_sign;
        to_sign.insert(to_sign.end(), x25519_pub.begin(), x25519_pub.end());
        to_sign.insert(to_sign.end(), kem_ct.begin(), kem_ct.end());
        to_sign.insert(to_sign.end(), iv, iv + AES_GCM_NONCE_SIZE);
        to_sign.insert(to_sign.end(), ciphertext.begin(), ciphertext.end());
        to_sign.insert(to_sign.end(), tag, tag + AES_GCM_TAG_SIZE);

        EVP_MD_CTX* mctx = EVP_MD_CTX_new();
        EVP_DigestSignInit_ex(mctx, NULL, NULL, NULL, NULL, client_dsa_key_, NULL);
        size_t sig_len = MLDSA65_SIGNATURE_SIZE;
        std::vector<unsigned char> signature(sig_len);
        EVP_DigestSign(mctx, signature.data(), &sig_len, to_sign.data(), to_sign.size());
        EVP_MD_CTX_free(mctx);
        signature.resize(sig_len);

        // 7. Assemble Binary Wire Packet according to Finora Spec Section 4
        WireHeader hdr;
        hdr.magic = htons(MAGIC);
        hdr.version = VERSION;
        hdr.payload_type = TYPE_FIX_TRAFFIC;
        hdr.session_id = htonl(session_id_);
        
        // Convert uint64_t to network byte order
        uint32_t seq_hi = htonl((uint32_t)(seq >> 32));
        uint32_t seq_lo = htonl((uint32_t)(seq & 0xFFFFFFFF));
        uint64_t net_seq = ((uint64_t)seq_hi << 32) | seq_lo;
        hdr.sequence_num = net_seq;

        hdr.payload_len = htonl((uint32_t)ciphertext.size());
        std::memcpy(hdr.iv, iv, AES_GCM_NONCE_SIZE);

        std::vector<unsigned char> wire_packet;
        const unsigned char* hdr_ptr = reinterpret_cast<const unsigned char*>(&hdr);
        wire_packet.insert(wire_packet.end(), hdr_ptr, hdr_ptr + sizeof(WireHeader));
        wire_packet.insert(wire_packet.end(), x25519_pub.begin(), x25519_pub.end());
        wire_packet.insert(wire_packet.end(), kem_ct.begin(), kem_ct.end());
        wire_packet.insert(wire_packet.end(), ciphertext.begin(), ciphertext.end());
        wire_packet.insert(wire_packet.end(), tag, tag + AES_GCM_TAG_SIZE);
        wire_packet.insert(wire_packet.end(), signature.begin(), signature.end());

        if (breakdown) {
            breakdown->magic = MAGIC;
            breakdown->version = VERSION;
            breakdown->payload_type = TYPE_FIX_TRAFFIC;
            breakdown->session_id = session_id_;
            breakdown->sequence_num = seq;
            breakdown->x25519_pubkey_hex = bytes_to_hex(x25519_pub);
            breakdown->kem_ciphertext_hex = bytes_to_hex(kem_ct);
            breakdown->aes_nonce_hex = bytes_to_hex(iv, AES_GCM_NONCE_SIZE);
            breakdown->aes_ciphertext_hex = bytes_to_hex(ciphertext);
            breakdown->aes_tag_hex = bytes_to_hex(tag, AES_GCM_TAG_SIZE);
            breakdown->mldsa_signature_hex = bytes_to_hex(signature);
            breakdown->total_wire_size = wire_packet.size();
            breakdown->plaintext_size = plaintext.size();
        }

        return wire_packet;
    }

    // Unwraps and verifies Finora Wire Format packet
    std::string unwrap_packet(const std::vector<unsigned char>& wire_packet) {
        using namespace finora;

        if (wire_packet.size() < sizeof(WireHeader) + X25519_PUBKEY_SIZE + MLKEM768_CIPHERTEXT_SIZE + AES_GCM_TAG_SIZE + MLDSA65_SIGNATURE_SIZE) {
            throw std::runtime_error("Finora: Malformed wire packet, below minimum header size");
        }

        const WireHeader* hdr = reinterpret_cast<const WireHeader*>(wire_packet.data());
        if (ntohs(hdr->magic) != MAGIC) {
            throw std::runtime_error("Finora: Invalid magic bytes (expected 0x464E)");
        }
        if (hdr->version != VERSION) {
            throw std::runtime_error("Finora: Unsupported protocol version");
        }

        size_t offset = sizeof(WireHeader);
        const unsigned char* x_pub = wire_packet.data() + offset;
        offset += X25519_PUBKEY_SIZE;

        const unsigned char* kem_ct = wire_packet.data() + offset;
        offset += MLKEM768_CIPHERTEXT_SIZE;

        uint32_t ct_len = ntohl(hdr->payload_len);
        if (offset + ct_len + AES_GCM_TAG_SIZE + MLDSA65_SIGNATURE_SIZE > wire_packet.size()) {
            throw std::runtime_error("Finora: Packet payload length out of bounds");
        }

        const unsigned char* ct_ptr = wire_packet.data() + offset;
        offset += ct_len;

        const unsigned char* tag_ptr = wire_packet.data() + offset;
        offset += AES_GCM_TAG_SIZE;

        const unsigned char* sig_ptr = wire_packet.data() + offset;

        // 1. Verify ML-DSA-65 Signature
        std::vector<unsigned char> signed_content;
        signed_content.insert(signed_content.end(), x_pub, x_pub + X25519_PUBKEY_SIZE);
        signed_content.insert(signed_content.end(), kem_ct, kem_ct + MLKEM768_CIPHERTEXT_SIZE);
        signed_content.insert(signed_content.end(), hdr->iv, hdr->iv + AES_GCM_NONCE_SIZE);
        signed_content.insert(signed_content.end(), ct_ptr, ct_ptr + ct_len);
        signed_content.insert(signed_content.end(), tag_ptr, tag_ptr + AES_GCM_TAG_SIZE);

        EVP_MD_CTX* vctx = EVP_MD_CTX_new();
        EVP_DigestVerifyInit_ex(vctx, NULL, NULL, NULL, NULL, client_dsa_key_, NULL);
        int sig_res = EVP_DigestVerify(vctx, sig_ptr, MLDSA65_SIGNATURE_SIZE, signed_content.data(), signed_content.size());
        EVP_MD_CTX_free(vctx);

        if (sig_res != 1) {
            throw std::runtime_error("Finora: ML-DSA-65 signature verification failed");
        }

        // 2. Compute Classical X25519 ECDH
        EVP_PKEY* client_x = EVP_PKEY_new_raw_public_key_ex(NULL, "X25519", NULL, x_pub, X25519_PUBKEY_SIZE);
        EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(static_peer_x25519_, NULL);
        EVP_PKEY_derive_init(derive_ctx);
        EVP_PKEY_derive_set_peer(derive_ctx, client_x);
        size_t x_ss_len = 0;
        EVP_PKEY_derive(derive_ctx, NULL, &x_ss_len);
        std::vector<unsigned char> x_ss(x_ss_len);
        EVP_PKEY_derive(derive_ctx, x_ss.data(), &x_ss_len);
        EVP_PKEY_CTX_free(derive_ctx);
        EVP_PKEY_free(client_x);

        // 3. Decapsulate ML-KEM-768 Shared Secret
        EVP_PKEY_CTX* dec_ctx = EVP_PKEY_CTX_new_from_pkey(NULL, server_kem_key_, NULL);
        EVP_PKEY_decapsulate_init(dec_ctx, NULL);
        size_t kem_ss_len = MLKEM768_SHARED_SECRET_SIZE;
        std::vector<unsigned char> kem_ss(kem_ss_len);
        EVP_PKEY_decapsulate(dec_ctx, kem_ss.data(), &kem_ss_len, kem_ct, MLKEM768_CIPHERTEXT_SIZE);
        EVP_PKEY_CTX_free(dec_ctx);

        // 4. Derive Hybrid Session Key: HKDF-SHA256
        std::vector<unsigned char> combined_ss = x_ss;
        combined_ss.insert(combined_ss.end(), kem_ss.begin(), kem_ss.end());

        EVP_KDF* kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
        EVP_KDF_CTX* kdf_ctx = EVP_KDF_CTX_new(kdf);
        EVP_KDF_free(kdf);
        OSSL_PARAM params[5];
        char md[] = "SHA256";
        params[0] = OSSL_PARAM_construct_utf8_string("digest", md, 0);
        params[1] = OSSL_PARAM_construct_octet_string("key", combined_ss.data(), combined_ss.size());
        params[2] = OSSL_PARAM_construct_octet_string("info", (void*)"Finora-v1-Traffic", 17);
        params[3] = OSSL_PARAM_construct_octet_string("salt", (void*)"Finora-PQC-Salt-2026", 20);
        params[4] = OSSL_PARAM_construct_end();

        std::vector<unsigned char> aes_key(32);
        EVP_KDF_derive(kdf_ctx, aes_key.data(), 32, params);
        EVP_KDF_CTX_free(kdf_ctx);

        // 5. Decrypt AES-256-GCM
        EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();
        EVP_DecryptInit_ex(d_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_DecryptInit_ex(d_ctx, NULL, NULL, aes_key.data(), hdr->iv);

        std::vector<unsigned char> decrypted(ct_len);
        int d_len1 = 0, d_len2 = 0;
        EVP_DecryptUpdate(d_ctx, decrypted.data(), &d_len1, ct_ptr, ct_len);
        EVP_CIPHER_CTX_ctrl(d_ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, (void*)tag_ptr);
        int d_ok = EVP_DecryptFinal_ex(d_ctx, decrypted.data() + d_len1, &d_len2);
        EVP_CIPHER_CTX_free(d_ctx);

        if (d_ok <= 0) {
            throw std::runtime_error("Finora: AES-256-GCM authentication tag verification failed");
        }

        decrypted.resize(d_len1 + d_len2);
        return std::string((char*)decrypted.data(), decrypted.size());
    }

    // Zero-Allocation Hot Path Streaming Wrap (§1.2 & §4)
    // Directly populates pre-allocated buffer with WireHeader + AES-GCM Ciphertext + Tag
    ssize_t wrap_streaming_packet(const uint8_t* plaintext, size_t pt_len,
                                  uint8_t* out_buf, size_t max_out_len,
                                  uint8_t payload_type = finora::TYPE_FIX_TRAFFIC) {
        using namespace finora;
        size_t total_required = sizeof(WireHeader) + pt_len + AES_GCM_TAG_SIZE;
        if (max_out_len < total_required) return -1;

        if (!has_cached_session_key_) {
            establish_session();
        }

        uint64_t seq = ++sequence_counter_;

        // 96-bit deterministic IV: 64-bit sequence counter + 32-bit salt (§3.3)
        uint8_t iv[AES_GCM_NONCE_SIZE];
        for (int i = 0; i < 8; ++i) {
            iv[i] = (uint8_t)((seq >> (56 - i * 8)) & 0xFF);
        }
        iv[8]  = (uint8_t)((salt_prefix_ >> 24) & 0xFF);
        iv[9]  = (uint8_t)((salt_prefix_ >> 16) & 0xFF);
        iv[10] = (uint8_t)((salt_prefix_ >> 8) & 0xFF);
        iv[11] = (uint8_t)(salt_prefix_ & 0xFF);

        uint8_t* ct_dest = out_buf + sizeof(WireHeader);
        int len1 = 0, len2 = 0;

        thread_local EVP_CIPHER_CTX* tl_enc_ctx = nullptr;
        if (!tl_enc_ctx) {
            tl_enc_ctx = EVP_CIPHER_CTX_new();
        }

        EVP_EncryptInit_ex(tl_enc_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_EncryptInit_ex(tl_enc_ctx, NULL, NULL, cached_session_key_, iv);
        EVP_EncryptUpdate(tl_enc_ctx, ct_dest, &len1, plaintext, static_cast<int>(pt_len));
        EVP_EncryptFinal_ex(tl_enc_ctx, ct_dest + len1, &len2);

        uint8_t* tag_dest = ct_dest + len1 + len2;
        EVP_CIPHER_CTX_ctrl(tl_enc_ctx, EVP_CTRL_GCM_GET_TAG, AES_GCM_TAG_SIZE, tag_dest);

        WireHeader* hdr = reinterpret_cast<WireHeader*>(out_buf);
        hdr->magic = htons(MAGIC);
        hdr->version = VERSION;
        hdr->payload_type = payload_type;
        hdr->session_id = htonl(session_id_);

        uint32_t seq_hi = htonl((uint32_t)(seq >> 32));
        uint32_t seq_lo = htonl((uint32_t)(seq & 0xFFFFFFFF));
        hdr->sequence_num = ((uint64_t)seq_hi << 32) | seq_lo;
        hdr->payload_len = htonl(static_cast<uint32_t>(len1 + len2));
        std::memcpy(hdr->iv, iv, AES_GCM_NONCE_SIZE);

        return static_cast<ssize_t>(sizeof(WireHeader) + len1 + len2 + AES_GCM_TAG_SIZE);
    }

    // Zero-Allocation Hot Path Streaming Unwrap (§1.2 & §4)
    ssize_t unwrap_streaming_packet(const uint8_t* wire_data, size_t wire_len,
                                    uint8_t* out_buf, size_t max_out_len,
                                    uint64_t* out_seq = nullptr,
                                    uint8_t* out_type = nullptr) {
        using namespace finora;
        if (wire_len < sizeof(WireHeader) + AES_GCM_TAG_SIZE) return -1;

        const WireHeader* hdr = reinterpret_cast<const WireHeader*>(wire_data);
        if (ntohs(hdr->magic) != MAGIC || hdr->version != VERSION) return -2;

        uint32_t ct_len = ntohl(hdr->payload_len);
        if (sizeof(WireHeader) + ct_len + AES_GCM_TAG_SIZE > wire_len) return -3;
        if (max_out_len < ct_len) return -4;

        if (!has_cached_session_key_) {
            establish_session();
        }

        if (out_seq) {
            uint32_t seq_hi = ntohl((uint32_t)(hdr->sequence_num >> 32));
            uint32_t seq_lo = ntohl((uint32_t)(hdr->sequence_num & 0xFFFFFFFF));
            *out_seq = ((uint64_t)seq_hi << 32) | seq_lo;
        }
        if (out_type) {
            *out_type = hdr->payload_type;
        }

        const uint8_t* ct_src = wire_data + sizeof(WireHeader);
        const uint8_t* tag_src = ct_src + ct_len;

        int d_len1 = 0, d_len2 = 0;
        thread_local EVP_CIPHER_CTX* tl_dec_ctx = nullptr;
        if (!tl_dec_ctx) {
            tl_dec_ctx = EVP_CIPHER_CTX_new();
        }

        EVP_DecryptInit_ex(tl_dec_ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_DecryptInit_ex(tl_dec_ctx, NULL, NULL, cached_session_key_, hdr->iv);
        EVP_DecryptUpdate(tl_dec_ctx, out_buf, &d_len1, ct_src, static_cast<int>(ct_len));
        EVP_CIPHER_CTX_ctrl(tl_dec_ctx, EVP_CTRL_GCM_SET_TAG, AES_GCM_TAG_SIZE, const_cast<uint8_t*>(tag_src));
        int ok = EVP_DecryptFinal_ex(tl_dec_ctx, out_buf + d_len1, &d_len2);

        if (ok <= 0) return -5; // GCM auth tag mismatch
        return static_cast<ssize_t>(d_len1 + d_len2);
    }
};
