// Created on Friday November 18th 2022 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#pragma once

#include "Memory/RingBuffer.hh"

#define LR_INVALID_EVENT ~0

namespace lr
{
using Event = u32;
template<typename TData, u64 TCount, bool TAtomicRingBuffer = false>
struct EventManager
{
    struct EventIdentifier
    {
        Event m_event;
        TData m_data;
    };

    using ring_buffer = typename eastl::conditional<
        TAtomicRingBuffer,
        Memory::RingBufferAtomic<EventIdentifier, TCount>,
        Memory::RingBuffer<EventIdentifier, TCount>>::type;

    bool peek()
    {
        return !m_ring_buffer.empty();
    }

    Event dispatch(TData &data_out)
    {
        ZoneScoped;

        if (EventIdentifier ident; m_ring_buffer.pop(ident))
        {
            data_out = ident.m_data;
            return ident.m_event;
        }

        return LR_INVALID_EVENT;
    }

    void push(Event event, TData &data)
    {
        ZoneScoped;

        EventIdentifier ident{ .m_event = event, .m_data = data };
        m_ring_buffer.push(ident);
    }

    ring_buffer m_ring_buffer;
};

}  // namespace lr