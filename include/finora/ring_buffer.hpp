#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <stdexcept>

namespace finora {

// Pre-allocated static packet frame size (64KB accommodates jumbo frames, full ML-DSA/ML-KEM wire envelopes)
constexpr size_t MAX_PACKET_SIZE = 65536;
constexpr size_t DEFAULT_RING_BUFFER_CAPACITY = 256; // 256 slots = 16MB pre-allocated at startup

struct alignas(64) PacketSlot {
    uint8_t  data[MAX_PACKET_SIZE];
    size_t   length{0};
    uint64_t sequence{0};
    uint64_t timestamp_ns{0};
    uint8_t  payload_type{0};
    bool     occupied{false};
};

/**
 * High-performance, lock-free single-producer single-consumer ring buffer.
 * Pre-allocates buffer pool at startup; guarantees ZERO heap allocation on the hot path.
 */
class ZeroAllocRingBuffer {
private:
    size_t capacity_{DEFAULT_RING_BUFFER_CAPACITY};
    std::unique_ptr<PacketSlot[]> slots_;
    alignas(64) std::atomic<size_t> head_{0}; // Producer write index
    alignas(64) std::atomic<size_t> tail_{0}; // Consumer read index

public:
    explicit ZeroAllocRingBuffer(size_t capacity = DEFAULT_RING_BUFFER_CAPACITY)
        : capacity_(capacity), slots_(std::make_unique<PacketSlot[]>(capacity)) {
        for (size_t i = 0; i < capacity_; ++i) {
            slots_[i].length = 0;
            slots_[i].sequence = 0;
            slots_[i].timestamp_ns = 0;
            slots_[i].occupied = false;
        }
    }

    // Direct pointer access for zero-copy socket reads
    PacketSlot* acquire_write_slot() noexcept {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (((h + 1) % capacity_) == (t % capacity_)) {
            return nullptr; // Buffer full, drop or backpressure
        }
        return &slots_[h % capacity_];
    }

    void commit_write_slot(PacketSlot* slot, size_t length, uint64_t seq, uint64_t ts, uint8_t type) noexcept {
        slot->length = length;
        slot->sequence = seq;
        slot->timestamp_ns = ts;
        slot->payload_type = type;
        slot->occupied = true;
        head_.store(head_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }

    PacketSlot* acquire_read_slot() noexcept {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (t == h) {
            return nullptr; // Buffer empty
        }
        return &slots_[t % capacity_];
    }

    void release_read_slot() noexcept {
        size_t t = tail_.load(std::memory_order_relaxed);
        slots_[t % capacity_].occupied = false;
        tail_.store(t + 1, std::memory_order_release);
    }

    size_t size() const noexcept {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_relaxed);
        return (h >= t) ? (h - t) : (capacity_ - (t - h));
    }

    bool is_empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }
};

} // namespace finora
