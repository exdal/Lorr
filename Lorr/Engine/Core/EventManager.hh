//
// Created on Friday 18th November 2022 by exdal
//

#pragma once

#include "Core/Memory/Allocator/LinearAllocator.hh"
#include "Core/Memory/MemoryUtils.hh"

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

            Memory::AllocatorDesc allocDesc = {};
            allocDesc.m_DataSize = memSize;

            m_Allocator.Init(allocDesc);
        }

        void Shutdown()
        {
            m_Allocator.Free(nullptr, true);
        }

        bool Peek(u32 &eventID)
        {
            ZoneScoped;

            bool completed = (m_Eventcount - 1) != eventID++;

            if (!completed)
            {
                m_Allocator.Free(nullptr, false);
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
            static_assert(sizeof(_Data) <= kMaxDataSize, "Maximum size of an event data must be `kMaxDataSize` bytes.");

            ZoneScoped;

            Memory::AllocationInfo allocInfo = {};
            allocInfo.m_Size = kMaxDataSize + sizeof(Event);
            allocInfo.m_Alignment = 1;
            allocInfo.m_pData = nullptr;

            if (!m_Allocator.Allocate(allocInfo))
                return;

            u64 offset = 0;
            Memory::CopyMem(allocInfo.m_pData, event, offset);
            Memory::CopyMem(allocInfo.m_pData, data, offset);

            m_Eventcount++;
        }

        u32 m_Eventcount = 0;
        Memory::LinearAllocator m_Allocator;
    };

}  // namespace lr