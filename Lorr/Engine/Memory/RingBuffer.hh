// Created on Tuesday May 16th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#pragma once

namespace lr::Memory
{
template<typename _Type, u32 _Count>
struct RingBuffer
{
    static constexpr u32 count = _Count;

    void push(const _Type &val)
    {
        m_Data[m_Head] = val;
        m_Head = (m_Head + 1) % count;

        if (m_Size == count)
            m_Tail = (m_Tail + 1) % count;
        else
            m_Size++;
    }

    bool pop(_Type &val)
    {
        if (m_Size == 0)
            return false;

        m_Size--;
        val = m_Data[m_Tail];
        m_Tail = (m_Tail + 1) % count;

        return true;
    }

    u32 m_Size = 0;
    u32 m_Head = 0;
    u32 m_Tail = 0;
    _Type m_Data[count] = {};
};

template<typename _Type, u32 _Count>
struct RingBufferAtomic
{
    static constexpr u32 count = _Count;

    void push(const _Type &val)
    {
        u32 head = m_Head.load(eastl::memory_order_acquire);
        m_Data[head] = val;
        head = (head + 1) % count;
        m_Head.store(head, eastl::memory_order_release);

        u32 size = m_Size.load(eastl::memory_order_acquire);

        if (size == count)
        {
            u32 tail = m_Tail.load(eastl::memory_order_acquire);
            tail = (tail + 1) % count;
            m_Tail.store(tail, eastl::memory_order_release);
        }
        else
        {
            size++;
            m_Size.store(size, eastl::memory_order_release);
        }
    }

    bool pop(_Type &val)
    {
        u32 size = m_Size.load(eastl::memory_order_acquire);
        if (size == 0)
            return false;

        size--;
        m_Size.store(size, eastl::memory_order_release);

        u32 tail = m_Tail.load(eastl::memory_order_acquire);
        val = m_Data[m_Tail];
        tail = (tail + 1) % count;
        m_Tail.store(tail, eastl::memory_order_release);

        return true;
    }

    eastl::atomic<u32> m_Size = 0;
    eastl::atomic<u32> m_Head = 0;
    eastl::atomic<u32> m_Tail = 0;
    _Type m_Data[count] = {};
};

}  // namespace lr::Memory