#include "finora/finora.h"
#include "finora/pqc_crypto.hpp"
#include "finora/ring_buffer.hpp"
#include "finora/state_guard.hpp"
#include "finora/codec.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <cstdlib>

struct finora_ctx_t {
    int sock_fd{-1};
    std::unique_ptr<PqcEngine> engine;
    std::unique_ptr<finora::ZeroAllocRingBuffer> ring_buf;
    std::unique_ptr<finora::SequenceSlidingWindow> seq_guard;
    std::unique_ptr<finora::AntiReplayFilter> anti_replay;
    
    alignas(64) uint8_t tx_buffer[finora::MAX_PACKET_SIZE];
    alignas(64) uint8_t rx_buffer[finora::MAX_PACKET_SIZE];
};

extern "C" {

finora_ctx_t* finora_client_connect(const char* remote_ip, uint16_t port, const char* config_path) {
    (void)config_path;
    if (!remote_ip) return nullptr;

    auto ctx = std::make_unique<finora_ctx_t>();
    ctx->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sock_fd < 0) return nullptr;

    // High performance socket tuning (§3.1)
    int opt = 1;
    setsockopt(ctx->sock_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef SO_BUSY_POLL
    int busy_poll_us = 50;
    setsockopt(ctx->sock_fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_us, sizeof(busy_poll_us));
#endif

    struct sockaddr_in saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    if (inet_pton(AF_INET, remote_ip, &saddr.sin_addr) <= 0) {
        close(ctx->sock_fd);
        return nullptr;
    }

    if (connect(ctx->sock_fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        close(ctx->sock_fd);
        return nullptr;
    }

    // Initialize cryptographic engine and state guard
    try {
        ctx->engine = std::make_unique<PqcEngine>();
        ctx->ring_buf = std::make_unique<finora::ZeroAllocRingBuffer>();
        ctx->seq_guard = std::make_unique<finora::SequenceSlidingWindow>();
        ctx->anti_replay = std::make_unique<finora::AntiReplayFilter>();
    } catch (...) {
        close(ctx->sock_fd);
        return nullptr;
    }

    return ctx.release();
}

int finora_send(finora_ctx_t* ctx, const uint8_t* payload, size_t length, uint8_t payload_type) {
    if (!ctx || !payload || length == 0 || ctx->sock_fd < 0) return -1;

    // Zero-allocation hot path wrap (§1.2 & §4)
    ssize_t wire_len = ctx->engine->wrap_streaming_packet(
        payload, length, ctx->tx_buffer, sizeof(ctx->tx_buffer), payload_type
    );

    if (wire_len <= 0) {
        // Fallback to full envelope wrap if streaming buffer is insufficient
        std::string plain(reinterpret_cast<const char*>(payload), length);
        auto wire_vec = ctx->engine->wrap_packet(plain);
        ssize_t sent = write(ctx->sock_fd, wire_vec.data(), wire_vec.size());
        return (sent == static_cast<ssize_t>(wire_vec.size())) ? 0 : -1;
    }

    ssize_t sent = write(ctx->sock_fd, ctx->tx_buffer, wire_len);
    return (sent == wire_len) ? 0 : -1;
}

ssize_t finora_recv(finora_ctx_t* ctx, uint8_t* buffer, size_t max_length) {
    if (!ctx || !buffer || max_length == 0 || ctx->sock_fd < 0) return -1;

    ssize_t bytes_read = read(ctx->sock_fd, ctx->rx_buffer, sizeof(ctx->rx_buffer));
    if (bytes_read <= 0) return bytes_read;

    // Fast streaming unwrap
    uint64_t seq = 0;
    uint8_t ptype = 0;
    ssize_t plain_len = ctx->engine->unwrap_streaming_packet(
        ctx->rx_buffer, bytes_read, buffer, max_length, &seq, &ptype
    );

    if (plain_len > 0) {
        // State guard validation
        if (seq > 0 && ctx->seq_guard) {
            auto res = ctx->seq_guard->validate_and_advance(seq);
            if (res != finora::SequenceSlidingWindow::CheckResult::ACCEPTED) {
                return -2; // Duplicate or out-of-order packet
            }
        }
        return plain_len;
    }

    // Fallback: full wire envelope unwrap
    try {
        std::vector<unsigned char> wire_vec(ctx->rx_buffer, ctx->rx_buffer + bytes_read);
        std::string verified = ctx->engine->unwrap_packet(wire_vec);
        if (verified.size() > max_length) return -3;
        std::memcpy(buffer, verified.data(), verified.size());
        return static_cast<ssize_t>(verified.size());
    } catch (...) {
        return -1;
    }
}

void finora_disconnect(finora_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->sock_fd >= 0) {
        shutdown(ctx->sock_fd, SHUT_RDWR);
        close(ctx->sock_fd);
        ctx->sock_fd = -1;
    }
    // Securely wipe buffers
    std::memset(ctx->tx_buffer, 0, sizeof(ctx->tx_buffer));
    std::memset(ctx->rx_buffer, 0, sizeof(ctx->rx_buffer));
    delete ctx;
}

} // extern "C"
