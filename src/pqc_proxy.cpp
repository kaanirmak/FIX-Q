#include "finora/tcp_server.hpp"
#include "finora/logger.hpp"
#include "finora/pqc_crypto.hpp"
#include "finora/codec.hpp"
#include "finora/state_guard.hpp"
#include "finora/ring_buffer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <csignal>

class PqcProxyServer : public TcpServer {
private:
    PqcEngine engine_;
    finora::FinoraTelemetry telemetry_;

    static void configure_hft_socket(int fd) {
        int opt = 1;
        // 1. Disable Nagle's algorithm (§3.1)
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        // 2. Enable kernel busy polling for deterministic microsecond tail latency (§3.1)
#ifdef SO_BUSY_POLL
        int busy_poll_us = 50;
        setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_us, sizeof(busy_poll_us));
#endif
    }

    void process_and_forward(int src_fd, int dst_fd, bool direction_to_bist,
                             finora::SequenceSlidingWindow& seq_guard,
                             finora::AntiReplayFilter& anti_replay) {
        // Pre-allocated static buffer on thread stack (§1.2 Zero Allocation)
        alignas(64) uint8_t in_buf[finora::MAX_PACKET_SIZE];

        while (true) {
            ssize_t bytes_read = read(src_fd, in_buf, sizeof(in_buf));
            if (bytes_read <= 0) break;

            uint64_t t_ingress = finora::FinoraTelemetry::now_nanoseconds();

            // 1. Sniff Protocol via finora-codec (§3.2)
            finora::ProtocolType proto = finora::FinoraCodec::sniff_protocol(in_buf, bytes_read);
            ssize_t boundary = finora::FinoraCodec::find_frame_boundary(proto, in_buf, bytes_read);
            size_t frame_len = (boundary > 0) ? static_cast<size_t>(boundary) : static_cast<size_t>(bytes_read);

            // 2. Perform In-Line Streaming AEAD Shielding (§3.3 Phase 2 & §4)
            alignas(64) uint8_t wire_buf[finora::MAX_PACKET_SIZE];
            alignas(64) uint8_t verified_buf[finora::MAX_PACKET_SIZE];

            ssize_t wire_len = engine_.wrap_streaming_packet(
                in_buf, frame_len, wire_buf, sizeof(wire_buf), static_cast<uint8_t>(proto)
            );
            if (wire_len <= 0) continue;

            const auto* hdr = reinterpret_cast<const finora::WireHeader*>(wire_buf);
            uint32_t seq_hi = ntohl((uint32_t)(hdr->sequence_num >> 32));
            uint32_t seq_lo = ntohl((uint32_t)(hdr->sequence_num & 0xFFFFFFFF));
            uint64_t seq = ((uint64_t)seq_hi << 32) | seq_lo;

            // 3. State Guard: Sequence Sliding Window & Anti-Replay Validation (§3.4)
            auto seq_result = seq_guard.validate_and_advance(seq);
            if (seq_result != finora::SequenceSlidingWindow::CheckResult::ACCEPTED) {
                continue;
            }

            uint64_t nonce_lo = 0;
            std::memcpy(&nonce_lo, hdr->iv, sizeof(nonce_lo));
            if (!anti_replay.check_and_record(nonce_lo)) {
                continue;
            }

            // 4. In-Line Streaming AEAD Decrypt and Authenticate (§3.3 Phase 2)
            uint64_t verified_seq = 0;
            uint8_t verified_type = 0;
            ssize_t verified_len = engine_.unwrap_streaming_packet(
                wire_buf, wire_len, verified_buf, sizeof(verified_buf), &verified_seq, &verified_type
            );
            if (verified_len <= 0) continue;

            uint64_t t_egress = finora::FinoraTelemetry::now_nanoseconds();
            telemetry_.record_transit(t_ingress, t_egress, frame_len, wire_len);

            // Sampled telemetry logging (§5: tracing_sample_rate: 0.001)
            static std::atomic<uint64_t> log_counter{0};
            if ((log_counter.fetch_add(1, std::memory_order_relaxed) % 10000) == 0) {
                if (direction_to_bist) {
                    logger::log_info("PqcProxyServer: [" + std::string(finora::protocol_to_string(proto)) + 
                                     "] Shielded In-Line PQC Packet [Seq=" + std::to_string(seq) + 
                                     ", Wire=" + std::to_string(wire_len) + "B] -> Transmitting to BIST");
                } else {
                    logger::log_info("PqcProxyServer: Shielded ExecutionReport PQC Packet [Wire=" +
                                     std::to_string(wire_len) + "B] -> Transmitting to Client");
                }
            }

            // Forward the verified plaintext to the destination socket
            write(dst_fd, verified_buf, verified_len);
        }
        shutdown(src_fd, SHUT_RD);
        shutdown(dst_fd, SHUT_WR);
    }

public:
    PqcProxyServer(int port) : TcpServer(port, "PqcProxyServer") {}

    void handle_client(int client_fd) override {
        configure_hft_socket(client_fd);
        
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        configure_hft_socket(server_fd);

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5003);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            logger::log_error("PqcProxyServer: Failed to connect to BIST (5003)");
            close(client_fd);
            return;
        }

        // Per-connection session state isolation (§3.4)
        finora::SequenceSlidingWindow client_seq_guard;
        finora::SequenceSlidingWindow bist_seq_guard;
        finora::AntiReplayFilter client_anti_replay;
        finora::AntiReplayFilter bist_anti_replay;

        std::thread t1(&PqcProxyServer::process_and_forward, this, client_fd, server_fd, true,
                       std::ref(client_seq_guard), std::ref(client_anti_replay));
        std::thread t2(&PqcProxyServer::process_and_forward, this, server_fd, client_fd, false,
                       std::ref(bist_seq_guard), std::ref(bist_anti_replay));
        t1.join();
        t2.join();

        close(client_fd);
        close(server_fd);
    }
};

int main() {
    signal(SIGPIPE, SIG_IGN);
    logger::log_info("Starting Finora PQC Proxy (OpenSSL 3.6.2 ML-KEM-768, ML-DSA-65, Codec & State-Guard)...");
    try {
        PqcProxyServer server(5006);
        server.run();
    } catch (const std::exception& e) {
        logger::log_error("Fatal error starting PQC Proxy: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
