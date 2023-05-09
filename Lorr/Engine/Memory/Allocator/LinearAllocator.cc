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
            "Linear memory allocator reached it's limit. Allocated {} in total and requested {}.",
            m_Size,
            size);
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

void LinearAllocator::Init(const AllocatorDesc &desc)
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

void *LinearAllocator::Allocate(u64 size, u32 alignment, void **ppAllocatorData)
{
    ZoneScoped;

    u64 offset = m_View.Allocate(size, alignment);
    if (offset == -1)
    {
        if (m_AutoGrowSize != 0)
        {
            m_View.m_Size += m_AutoGrowSize;
            Reserve(m_View.m_Size);
            offset = m_View.Allocate(size, alignment);
        }
        else
        {
            return nullptr;
        }
    }

    return m_pData + offset;
}

void LinearAllocator::Free(void *pData, bool freeData)
{
    ZoneScoped;

    m_View.Free();

    if (freeData)
        Memory::Release(m_pData);
}

void LinearAllocator::Reserve(u64 size)
{
    ZoneScoped;

    Memory::Reallocate(m_pData, size);
}

}  // namespace lr::Memory