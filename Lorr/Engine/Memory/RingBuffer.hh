#pragma once

#include <atomic>

namespace lr::Memory {
template<typename T, u32 Count>
struct RingBuffer {
    static_assert(!(Count & 1), "Count must be power of 2.");
    static constexpr u32 MASK = Count - 1;

    void push(const T &val)
    {
        ZoneScoped;

        m_Data[m_Tail++ & MASK] = val;
    }

    bool pop(T &val)
    {
        if (m_Tail > m_Head) {
            val = m_Data[m_Head++ & MASK];
            return true;
        }

        return false;
    }

    bool empty() { return m_Head == m_Tail; }

    u32 m_Head = 0;
    u32 m_Tail = 0;
    T m_Data[Count] = {};
};

template<typename T, u32 Count>
struct RingBufferAtomic {
    static_assert(!(Count & 1), "Count must be power of 2.");
    static constexpr u32 MASK = Count - 1;

    // Produced by one(producer) thread.
    // TODO: This should give a feedback to user if val is pushed or not, so it won't drop
    // other valuables.
    /// This function does not push head forward when tail reaches head.
    void push(const T &val)
    {
        u32 tail = m_Tail.load(std::memory_order_acquire);
        m_Data[tail & MASK] = val;
        m_Tail.store(tail + 1, std::memory_order_release);
    }

    // Consumed by worker threads
    bool pop(T &val)
    {
        if (empty())
            return false;

        u32 head = m_Head.load(std::memory_order_acquire);
        u32 tail = m_Tail.load(std::memory_order_acquire);

        if (tail > head) {
            val = m_Data[head & MASK];

            if (m_Head.compare_exchange_weak(
                    head, head + 1, std::memory_order_release, std::memory_order_relaxed))
                return true;
        }

        return false;
    }

    bool empty()
    {
        return m_Head.load(std::memory_order_acquire)
               == m_Tail.load(std::memory_order_acquire);
    }

    std::atomic<u32> m_Head = 0;
    std::atomic<u32> m_Tail = 0;
    T m_Data[Count] = {};
};

}  // namespace lr::Memory
