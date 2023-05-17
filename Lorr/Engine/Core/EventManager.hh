// Created on Friday November 18th 2022 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#pragma once

#include "Memory/Allocator/LinearAllocator.hh"
#include "Memory/MemoryUtils.hh"

#define LR_INVALID_EVENT_ID ~0
#define LR_GET_EVENT_DATA(type, pTargetData, dataName) type &dataName = *(type *)pTargetData

namespace lr
{
using Event = u32;

// * Notes:
// * Total event data must be `kMaxDataSize`.
template<typename _Data>
struct EventManager
{
    static constexpr u32 kMaxDataSize = sizeof(_Data);

    void Init(u32 memSize = 0xfff)
    {
        ZoneScoped;

        m_Allocator.Init({ .m_DataSize = memSize });
    }

    void Shutdown()
    {
        ZoneScoped;
        
        m_Allocator.Free(true);
    }

    bool Peek(u32 &eventID)
    {
        ZoneScoped;

        bool completed = (m_Eventcount - 1) != eventID++;

        if (!completed)
        {
            m_Allocator.Free(false);
            m_Eventcount = 0;
        }

        return completed;
    }

    Event Dispatch(u32 eventID, _Data &dataOut)
    {
        ZoneScoped;

        u8 *pData = m_Allocator.m_pData + ((kMaxDataSize + sizeof(Event)) * eventID);
        dataOut = *(_Data *)(pData + sizeof(Event));

        return *(Event *)pData;
    }

    void Push(Event event, _Data &data)
    {
        static_assert(
            sizeof(_Data) <= kMaxDataSize, "Maximum size of an event data must be `kMaxDataSize` bytes.");

        ZoneScoped;

        void *pData = m_Allocator.Allocate(kMaxDataSize + sizeof(Event));
        if (!pData)
            return;

        u64 offset = 0;
        Memory::CopyMem(pData, event, offset);
        Memory::CopyMem(pData, data, offset);

        m_Eventcount++;
    }

    u32 m_Eventcount = 0;
    Memory::LinearAllocator m_Allocator;
};

}  // namespace lr