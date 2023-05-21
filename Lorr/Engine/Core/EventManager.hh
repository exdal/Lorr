// Created on Friday November 18th 2022 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#pragma once

#include "Memory/RingBuffer.hh"

#define LR_INVALID_EVENT ~0

namespace lr
{
using Event = u32;
template<typename _Data, u64 _Count, bool _AtomicRingBuffer = false>
struct EventManager
{
    struct EventIdentifier
    {
        Event m_Event;
        _Data m_Data;
    };

    using _RingBuffer = eastl::conditional<
        _AtomicRingBuffer,
        Memory::RingBufferAtomic<EventIdentifier, _Count>,
        Memory::RingBuffer<EventIdentifier, _Count>>::type;

    bool Peek()
    {
        return !m_RingBuffer.empty();
    }

    Event Dispatch(_Data &dataOut)
    {
        ZoneScoped;

        EventIdentifier ident;
        if (m_RingBuffer.pop(ident))
        {
            dataOut = ident.m_Data;
            return ident.m_Event;
        }
        else
        {
            return LR_INVALID_EVENT;
        }
    }

    void Push(Event event, _Data &data)
    {
        ZoneScoped;

        EventIdentifier ident{ .m_Event = event, .m_Data = data };
        m_RingBuffer.push(ident);
    }

    _RingBuffer m_RingBuffer;
};

}  // namespace lr