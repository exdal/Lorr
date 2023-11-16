#include "CommandBatcher.hh"

namespace lr::Renderer
{
using namespace Graphics;

CommandBatcher::CommandBatcher(CommandList *pList)
    : m_pList(pList)
{
}

CommandBatcher::~CommandBatcher()
{
    FlushBarriers();
}

void CommandBatcher::InsertMemoryBarrier(MemoryBarrier &barrier)
{
    ZoneScoped;

    if (m_MemoryBarrierCount == m_MemoryBarriers.max_size())
        FlushBarriers();

    m_MemoryBarriers[m_MemoryBarrierCount++] = barrier;
}

void CommandBatcher::InsertImageBarrier(ImageBarrier &barrier)
{
    ZoneScoped;

    if (m_ImageBarrierCount == m_ImageBarriers.max_size())
        FlushBarriers();

    m_ImageBarriers[m_ImageBarrierCount++] = barrier;
}

void CommandBatcher::FlushBarriers()
{
    DependencyInfo depInfo(
        { m_ImageBarriers.data(), m_ImageBarrierCount },
        { m_MemoryBarriers.data(), m_MemoryBarrierCount });
    m_pList->SetPipelineBarrier(&depInfo);

    m_MemoryBarrierCount = 0;
    m_ImageBarrierCount = 0;
}
}  // namespace lr::Renderer