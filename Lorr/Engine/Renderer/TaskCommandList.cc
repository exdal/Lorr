#include "TaskCommandList.hh"

namespace lr::Renderer
{
using namespace Graphics;

TaskCommandList::TaskCommandList(CommandList *list)
    : m_list(list)
{
}

TaskCommandList::~TaskCommandList()
{
    flush_barriers();
}

void TaskCommandList::insert_memory_barrier(MemoryBarrier &barrier)
{
    ZoneScoped;

    if (m_memory_barrier_count == m_memory_barriers.max_size())
        flush_barriers();

    m_memory_barriers[m_memory_barrier_count++] = barrier;
}

void TaskCommandList::insert_image_barrier(ImageBarrier &barrier)
{
    ZoneScoped;

    if (m_image_barrier_count == m_image_barriers.max_size())
        flush_barriers();

    m_image_barriers[m_image_barrier_count++] = barrier;
}

void TaskCommandList::flush_barriers()
{
    ZoneScoped;

    if (m_image_barrier_count == 0 && m_memory_barrier_count == 0)
        return;

    DependencyInfo depInfo(
        { m_image_barriers.data(), m_image_barrier_count },
        { m_memory_barriers.data(), m_memory_barrier_count });
    m_list->set_pipeline_barrier(&depInfo);

    m_memory_barrier_count = 0;
    m_image_barrier_count = 0;
}
} // namespace lr::Renderer
