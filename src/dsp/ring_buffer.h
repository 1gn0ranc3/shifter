#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace shifter {

// Lock-free single-producer single-consumer ring buffer for float samples.
// Writer is the audio thread; reader is a background learning thread.
// Capacity is fixed at compile time and must be a power of two.
// One slot is reserved to disambiguate full vs. empty, so usable capacity is Capacity - 1.
template <std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool write(float value) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & mask_;
        if (next == tail_.load(std::memory_order_acquire))
            return false;
        buffer_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool read(float& out) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false;
        out = buffer_[tail];
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity - 1; }

private:
    static constexpr std::size_t mask_ = Capacity - 1;
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<float, Capacity> buffer_{};
};

} // namespace shifter
