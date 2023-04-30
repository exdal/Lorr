#include "LinearAllocator.hh"

#include "Memory/MemoryUtils.hh"

namespace lr::Memory
{
    void LinearAllocatorView::Init(u64 size)
    {
        ZoneScoped;

        m_Size = size;
    }

    bool LinearAllocatorView::CanAllocate(u64 size, u32 alignment)
    {
        ZoneScoped;

        u32 alignedSize = Memory::AlignUp(size, alignment);

        if (m_Offset + alignedSize > m_Size)
            return false;

        return true;
    }

    u64 LinearAllocatorView::Allocate(u64 size, u32 alignment)
    {
        ZoneScoped;

        if (!CanAllocate(size, alignment))
        {
            LOG_ERROR(
                "Linear memory allocator reached it's limit. Allocated {} and requested {}.", m_Size, size);
            return -1;
        }

        u32 alignedSize = Memory::AlignUp(size, alignment);

        u64 off = m_Offset;
        m_Offset += alignedSize;

        return off;
    }

    void LinearAllocatorView::Free()
    {
        ZoneScoped;

        m_Offset = 0;
    }

    void LinearAllocator::Init(AllocatorDesc &desc)
    {
        ZoneScoped;

        m_View.Init(desc.m_DataSize);
        m_pData = (u8 *)desc.m_pInitialData;
        m_SelfAllocated = !(bool)m_pData;

        if (!m_pData)
            m_pData = Memory::Allocate<u8>(desc.m_DataSize);
    }

    void LinearAllocator::Delete()
    {
        ZoneScoped;

        if (m_SelfAllocated)
            Memory::Release(m_pData);
    }

    bool LinearAllocator::CanAllocate(u64 size, u32 alignment)
    {
        ZoneScoped;

        return m_View.CanAllocate(size, alignment);
    }

    bool LinearAllocator::Allocate(AllocationInfo &info)
    {
        ZoneScoped;

        u64 offset = m_View.Allocate(info.m_Size, info.m_Alignment);
        if (offset == -1)
        {
            if (m_AutoGrowSize != 0)
            {
                Grow(m_AutoGrowSize);
                m_View.m_Size += m_AutoGrowSize;
                offset = m_View.Allocate(info.m_Size, info.m_Alignment);
            }
            else
            {
                return false;
            }
        }

        void *pData = m_pData + offset;

        if (!info.m_pData)
            info.m_pData = pData;
        else
            memcpy(pData, info.m_pData, info.m_Size);

        return true;
    }

    void LinearAllocator::Free(void *pData, bool freeData)
    {
        ZoneScoped;

        m_View.Free();

        if (freeData)
            Memory::Release(m_pData);
    }

    void LinearAllocator::Grow(u64 size)
    {
        ZoneScoped;

        Memory::Reallocate(m_pData, size);
    }

}  // namespace lr::Memory