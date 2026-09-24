#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <vector>
#include <mutex>

namespace finora {

/**
 * 64-bit Sequence Sliding Window (§3.4)
 * Rejects duplicate, out-of-order, or excessively lagged packets.
 */
class SequenceSlidingWindow {
public:
    static constexpr size_t WINDOW_SIZE = 4096; // 4096-packet sliding window
    static constexpr size_t BITMAP_WORDS = WINDOW_SIZE / 64;

private:
    uint64_t highest_seq_{0};
    std::array<uint64_t, BITMAP_WORDS> bitmap_{};
    std::atomic<uint64_t> dropped_duplicates_{0};
    std::atomic<uint64_t> dropped_too_old_{0};
    std::atomic<uint64_t> accepted_packets_{0};
    std::mutex mtx_;

public:
    SequenceSlidingWindow() {
        reset(0);
    }

    void reset(uint64_t initial_seq) {
        std::lock_guard<std::mutex> lock(mtx_);
        highest_seq_ = initial_seq;
        bitmap_.fill(0);
        if (initial_seq > 0) {
            bitmap_[0] = 1; // Mark initial
        }
    }

    enum class CheckResult {
        ACCEPTED,
        DUPLICATE,
        TOO_OLD
    };

    CheckResult validate_and_advance(uint64_t seq) {
        std::lock_guard<std::mutex> lock(mtx_);

        if (highest_seq_ == 0 && accepted_packets_.load(std::memory_order_relaxed) == 0) {
            highest_seq_ = seq;
            bitmap_[0] = 1;
            accepted_packets_.fetch_add(1, std::memory_order_relaxed);
            return CheckResult::ACCEPTED;
        }

        if (seq > highest_seq_) {
            uint64_t diff = seq - highest_seq_;
            if (diff >= WINDOW_SIZE) {
                // Large jump: clear entire bitmap
                bitmap_.fill(0);
            } else {
                // Shift bitmap left by diff bits
                shift_bitmap_left(diff);
            }
            highest_seq_ = seq;
            set_bit(0); // 0 corresponds to highest_seq_
            accepted_packets_.fetch_add(1, std::memory_order_relaxed);
            return CheckResult::ACCEPTED;
        }

        uint64_t diff = highest_seq_ - seq;
        if (diff >= WINDOW_SIZE) {
            dropped_too_old_.fetch_add(1, std::memory_order_relaxed);
            return CheckResult::TOO_OLD;
        }

        // Within window: check if bit is already set (duplicate)
        if (test_bit(diff)) {
            dropped_duplicates_.fetch_add(1, std::memory_order_relaxed);
            return CheckResult::DUPLICATE;
        }

        set_bit(diff);
        accepted_packets_.fetch_add(1, std::memory_order_relaxed);
        return CheckResult::ACCEPTED;
    }

    uint64_t highest_sequence() const { return highest_seq_; }
    uint64_t dropped_duplicates() const { return dropped_duplicates_.load(std::memory_order_relaxed); }
    uint64_t dropped_too_old() const { return dropped_too_old_.load(std::memory_order_relaxed); }
    uint64_t accepted_count() const { return accepted_packets_.load(std::memory_order_relaxed); }

private:
    void shift_bitmap_left(uint64_t shift) {
        if (shift >= WINDOW_SIZE) {
            bitmap_.fill(0);
            return;
        }
        size_t word_shift = shift / 64;
        size_t bit_shift = shift % 64;

        for (int i = static_cast<int>(BITMAP_WORDS) - 1; i >= 0; --i) {
            uint64_t high = 0;
            uint64_t low = 0;

            if (static_cast<size_t>(i) >= word_shift) {
                high = bitmap_[i - word_shift];
                if (bit_shift > 0 && static_cast<size_t>(i) >= word_shift + 1) {
                    low = bitmap_[i - word_shift - 1];
                }
            }

            if (bit_shift == 0) {
                bitmap_[i] = high;
            } else {
                bitmap_[i] = (high << bit_shift) | (low >> (64 - bit_shift));
            }
        }
    }

    void set_bit(uint64_t index) {
        size_t word = index / 64;
        size_t bit = index % 64;
        if (word < BITMAP_WORDS) {
            bitmap_[word] |= (1ULL << bit);
        }
    }

    bool test_bit(uint64_t index) const {
        size_t word = index / 64;
        size_t bit = index % 64;
        if (word < BITMAP_WORDS) {
            return (bitmap_[word] & (1ULL << bit)) != 0;
        }
        return true;
    }
};

/**
 * Nonce-based Anti-Replay LRU Filter (§3.4)
 * Stores recent 12-byte IVs / 64-bit nonces to block replay attacks.
 */
class AntiReplayFilter {
private:
    static constexpr size_t FILTER_CAPACITY = 8192;
    std::array<uint64_t, FILTER_CAPACITY> nonce_ring_{};
    size_t ring_head_{0};
    std::atomic<uint64_t> replay_count_{0};
    std::mutex mtx_;

public:
    AntiReplayFilter() {
        nonce_ring_.fill(0);
    }

    bool check_and_record(uint64_t nonce) {
        if (nonce == 0) return false;
        std::lock_guard<std::mutex> lock(mtx_);

        // Fast scan in ring buffer
        for (size_t i = 0; i < FILTER_CAPACITY; ++i) {
            if (nonce_ring_[i] == nonce) {
                replay_count_.fetch_add(1, std::memory_order_relaxed);
                return false; // Replay detected!
            }
        }

        nonce_ring_[ring_head_] = nonce;
        ring_head_ = (ring_head_ + 1) % FILTER_CAPACITY;
        return true; // Fresh nonce
    }

    uint64_t replays_mitigated() const {
        return replay_count_.load(std::memory_order_relaxed);
    }
};

/**
 * High-resolution nanosecond telemetry & percentiles (§3.4)
 */
class FinoraTelemetry {
private:
    static constexpr size_t LATENCY_HISTORY_CAP = 10000;
    std::array<double, LATENCY_HISTORY_CAP> latencies_us_{};
    size_t history_idx_{0};
    std::atomic<uint64_t> total_bytes_in_{0};
    std::atomic<uint64_t> total_bytes_out_{0};
    std::mutex mtx_;

public:
    static uint64_t now_nanoseconds() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }

    void record_transit(uint64_t start_ns, uint64_t end_ns, size_t bytes_in, size_t bytes_out) {
        if (end_ns > start_ns) {
            double us = (end_ns - start_ns) / 1000.0;
            std::lock_guard<std::mutex> lock(mtx_);
            latencies_us_[history_idx_ % LATENCY_HISTORY_CAP] = us;
            history_idx_++;
        }
        total_bytes_in_.fetch_add(bytes_in, std::memory_order_relaxed);
        total_bytes_out_.fetch_add(bytes_out, std::memory_order_relaxed);
    }

    struct LatencyPercentiles {
        double p50{0.0};
        double p90{0.0};
        double p99{0.0};
        double p999{0.0};
        size_t samples{0};
    };

    LatencyPercentiles get_percentiles() {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t n = std::min(history_idx_, LATENCY_HISTORY_CAP);
        if (n == 0) return {};

        std::vector<double> sorted(latencies_us_.begin(), latencies_us_.begin() + n);
        std::sort(sorted.begin(), sorted.end());

        LatencyPercentiles lp;
        lp.samples = n;
        lp.p50 = sorted[static_cast<size_t>(n * 0.50)];
        lp.p90 = sorted[static_cast<size_t>(n * 0.90)];
        lp.p99 = sorted[static_cast<size_t>(n * 0.99)];
        lp.p999 = sorted[static_cast<size_t>(n * 0.999)];
        return lp;
    }

    uint64_t total_bytes_in() const { return total_bytes_in_.load(std::memory_order_relaxed); }
    uint64_t total_bytes_out() const { return total_bytes_out_.load(std::memory_order_relaxed); }
};

} // namespace finora
