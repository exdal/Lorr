// Created on Tuesday May 16th 2023 by exdal
// Last modified on Monday June 12th 2023 by exdal

#pragma once

namespace lr::Memory
{
template<typename _Type, u32 _Count>
struct RingBuffer
{
    static_assert(!(_Count & 1), "_Count must be power of 2.");
    static constexpr u32 MASK = _Count - 1;

    void push(const _Type &val)
    {
        ZoneScoped;

        m_Data[m_Tail++ & MASK] = val;
    }

    bool pop(_Type &val)
    {
        if (m_Tail > m_Head)
        {
            val = m_Data[m_Head++ & MASK];
            return true;
        }

        return false;
    }

    bool empty() { return m_Head == m_Tail; }

    u32 m_Head = 0;
    u32 m_Tail = 0;
    _Type m_Data[_Count] = {};
};

template<typename _Type, u32 _Count>
struct RingBufferAtomic
{
    static_assert(!(_Count & 1), "_Count must be power of 2.");
    static constexpr u32 MASK = _Count - 1;

    // Produced by one(producer) thread.
    // TODO: This should give a feedback to user if val is pushed or not, so it won't drop other valuables.
    /// This function does not push head forward when tail reaches head.
    void push(const _Type &val)
    {
        u32 tail = m_Tail.load(eastl::memory_order_acquire);
        m_Data[tail & MASK] = val;
        m_Tail.store(tail + 1, eastl::memory_order_release);
    }

    // Consumed by worker threads
    bool pop(_Type &val)
    {
        if (empty())
            return false;

        u32 head = m_Head.load(eastl::memory_order_acquire);
        u32 tail = m_Tail.load(eastl::memory_order_acquire);

        if (tail > head)
        {
            val = m_Data[head & MASK];

            if (m_Head.compare_exchange_weak(
                    head, head + 1, eastl::memory_order_release, eastl::memory_order_relaxed))
                return true;
        }

        return false;
    }

    bool empty()
    {
        return m_Head.load(eastl::memory_order_acquire) == m_Tail.load(eastl::memory_order_acquire);
    }

    eastl::atomic<u32> m_Head = 0;
    eastl::atomic<u32> m_Tail = 0;
    _Type m_Data[_Count] = {};
};

}  // namespace lr::Memory