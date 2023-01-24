#include "BaseCommandList.hh"

#include <eathread/eathread_thread.h>

#undef LOG_SET_NAME
#define LOG_SET_NAME "BASECMD"

namespace lr::Graphics
{
    void CommandListPool::Init()
    {
        ZoneScoped;

        m_DirectListMask.store(UINT32_MAX, eastl::memory_order_release);
        m_DirectFenceMask.store(0, eastl::memory_order_release);
        
        m_ComputeListMask.store(UINT16_MAX, eastl::memory_order_release);
        m_CopyListMask.store(UINT8_MAX, eastl::memory_order_release);
    }

    CommandList *CommandListPool::Acquire(CommandListType type)
    {
        ZoneScoped;

        CommandList *pList = nullptr;
        u32 setBitPosition = 0;

        auto GetListFromMask = [&](auto &&maskAtomic, auto &&listArray) {
            auto mask = maskAtomic.load(eastl::memory_order_acquire);
            while (!mask)
            {
                mask = maskAtomic.load(eastl::memory_order_acquire);
            }

            setBitPosition = Memory::GetLSB(mask);
            decltype(mask) newMask = 0;

            do
            {
                newMask = mask ^ (1 << setBitPosition);
            } while (!maskAtomic.compare_exchange_weak(mask, newMask, eastl::memory_order_release, eastl::memory_order_relaxed));

            pList = listArray[setBitPosition];
        };

        switch (type)
        {
            case CommandListType::Direct: GetListFromMask(m_DirectListMask, m_DirectLists); break;
            case CommandListType::Compute: GetListFromMask(m_ComputeListMask, m_ComputeLists); break;
            case CommandListType::Copy: GetListFromMask(m_CopyListMask, m_CopyLists); break;

            default: break;
        }

        return pList;
    }

    void CommandListPool::Release(CommandList *pList)
    {
        ZoneScoped;

        auto PlaceIntoArray = [&](auto &&maskAtomic, auto &&listArray) {
            for (u32 i = 0; i < listArray.count; i++)
            {
                if (listArray[i] == pList)
                {
                    // We really don't care if the bit is set before, it shouldn't happen anyway

                    auto mask = maskAtomic.load(eastl::memory_order_acquire);
                    decltype(mask) newMask = 0;

                    do
                    {
                        newMask = mask | (1 << i);
                    } while (!maskAtomic.compare_exchange_weak(mask, newMask, eastl::memory_order_release, eastl::memory_order_relaxed));

                    return;
                }
            }

            LOG_ERROR("Trying to push unknown Command List into the Command List pool.");
        };

        switch (pList->m_Type)
        {
            case CommandListType::Direct: PlaceIntoArray(m_DirectListMask, m_DirectLists); break;
            case CommandListType::Compute: PlaceIntoArray(m_ComputeListMask, m_ComputeLists); break;
            case CommandListType::Copy: PlaceIntoArray(m_CopyListMask, m_CopyLists); break;

            default: break;
        }
    }

    void CommandListPool::SignalFence(CommandList *pList)
    {
        ZoneScoped;

        for (u32 i = 0; i < m_DirectLists.count; i++)
        {
            if (m_DirectLists[i] == pList)
            {
                u32 mask = m_DirectFenceMask.load(eastl::memory_order_acquire);
                u32 newMask = 0;

                do
                {
                    newMask = mask | (1 << i);
                } while (!m_DirectFenceMask.compare_exchange_weak(mask, newMask, eastl::memory_order_release, eastl::memory_order_relaxed));
            }
        }
    }

    void CommandListPool::ReleaseFence(u32 index)
    {
        ZoneScoped;

        u32 mask = m_DirectFenceMask.load(eastl::memory_order_acquire);
        u32 newMask = 0;

        do
        {
            newMask = mask ^ (1 << index);
        } while (!m_DirectFenceMask.compare_exchange_weak(mask, newMask, eastl::memory_order_release, eastl::memory_order_relaxed));
    }

    void CommandListPool::WaitForAll()
    {
        ZoneScoped;

        u32 mask = m_DirectListMask.load(eastl::memory_order_acquire);

        while (mask != UINT32_MAX)
        {
            EAProcessorPause();
        }
    }

}  // namespace lr::Graphics