// Created on Thursday May 18th 2023 by exdal
// Last modified on Thursday May 18th 2023 by exdal

#pragma once

namespace lr
{
struct SpinLock
{
    void Lock()
    {
        ZoneScoped;

        while (m_Flag.test_and_set(eastl::memory_order_acquire))
            ;
    }

    void Unlock()
    {
        ZoneScoped;

        m_Flag.clear(eastl::memory_order_release);
    }

    bool IsLocked()
    {
        ZoneScoped;

        return m_Flag.test(eastl::memory_order_acquire);
    }

    eastl::atomic_flag m_Flag = {};
};

class ScopedSpinLock
{
public:
    ScopedSpinLock(SpinLock &spinLock)
        : m_SpinLock(spinLock)
    {
        m_SpinLock.Lock();
    }

    ~ScopedSpinLock() { m_SpinLock.Unlock(); }

    SpinLock &m_SpinLock;
};

}  // namespace lr