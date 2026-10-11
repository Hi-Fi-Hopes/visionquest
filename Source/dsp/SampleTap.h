#pragma once
// Lock-free tap from the audio thread to the UI. The audio thread pushes every
// block; the UI reads the newest N samples whenever it draws a frame. No locks,
// no allocation on the audio thread. A frame drawn while a block is being
// written may see a few samples from the block before, which is harmless here.

#include <atomic>
#include <cstdint>
#include <memory>

namespace vq
{

class SampleTap
{
public:
    static constexpr int capacity = 1 << 15;   // ~0.7 s at 48 kHz
    static constexpr int mask = capacity - 1;

    SampleTap()
        : left (new std::atomic<float>[capacity]), right (new std::atomic<float>[capacity])
    {
        for (int i = 0; i < capacity; ++i) { left[i].store (0.0f); right[i].store (0.0f); }
    }

    void push (const float* l, const float* r, int n)
    {
        const uint64_t w = written.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((w + (uint64_t) i) & mask);
            left[idx].store (l[i], std::memory_order_relaxed);
            right[idx].store (r[i], std::memory_order_relaxed);
        }
        written.store (w + (uint64_t) n, std::memory_order_release);
    }

    // Copies the newest n samples (oldest first). Returns the total number of
    // samples ever written, so the caller can tell how many are new.
    uint64_t readLatest (float* l, float* r, int n) const
    {
        const uint64_t w = written.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((w - (uint64_t) n + (uint64_t) i) & mask);
            l[i] = left[idx].load (std::memory_order_relaxed);
            r[i] = right[idx].load (std::memory_order_relaxed);
        }
        return w;
    }

    uint64_t totalWritten() const { return written.load (std::memory_order_acquire); }

private:
    std::unique_ptr<std::atomic<float>[]> left, right;
    std::atomic<uint64_t> written { 0 };
};

} // namespace vq
