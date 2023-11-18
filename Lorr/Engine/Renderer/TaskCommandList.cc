#include "TaskCommandList.hh"

namespace lr::Renderer
{
using namespace Graphics;

TaskCommandList::TaskCommandList(CommandList *pList)
    : m_pList(pList)
{
}

TaskCommandList::~TaskCommandList()
{
    FlushBarriers();
}

void TaskCommandList::InsertMemoryBarrier(MemoryBarrier &barrier)
{
    ZoneScoped;

    if (m_MemoryBarrierCount == m_MemoryBarriers.max_size())
        FlushBarriers();

    m_MemoryBarriers[m_MemoryBarrierCount++] = barrier;
}

void TaskCommandList::InsertImageBarrier(ImageBarrier &barrier)
{
    ZoneScoped;

    if (m_ImageBarrierCount == m_ImageBarriers.max_size())
        FlushBarriers();

    m_ImageBarriers[m_ImageBarrierCount++] = barrier;
}

void TaskCommandList::FlushBarriers()
{
    if (m_ImageBarrierCount == 0 && m_MemoryBarrierCount == 0)
        return;

    DependencyInfo depInfo(
        { m_ImageBarriers.data(), m_ImageBarrierCount },
        { m_MemoryBarriers.data(), m_MemoryBarrierCount });
    m_pList->SetPipelineBarrier(&depInfo);

    m_MemoryBarrierCount = 0;
    m_ImageBarrierCount = 0;
}
}  // namespace lr::Renderer