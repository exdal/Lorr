#include "CommandBatcher.hh"

#include "TaskGraph.hh"

namespace lr::Graphics
{
CommandBatcher::CommandBatcher(CommandList *command_list)
    : m_command_list(command_list)
{
}

CommandBatcher::~CommandBatcher()
{
    flush_barriers();
}

void CommandBatcher::insert_memory_barrier(MemoryBarrier &barrier)
{
    ZoneScoped;

    if (m_memory_barriers.full())
        flush_barriers();

    m_memory_barriers.push_back(barrier);
}

void CommandBatcher::insert_image_barrier(ImageBarrier &barrier)
{
    ZoneScoped;

    if (m_image_barriers.full())
        flush_barriers();

    m_image_barriers.push_back(barrier);
}

void CommandBatcher::flush_barriers()
{
    ZoneScoped;

    if (m_image_barriers.empty() && m_memory_barriers.empty())
        return;

    DependencyInfo dep_info(m_image_barriers, m_memory_barriers);
    m_command_list->set_pipeline_barrier(&dep_info);

    m_image_barriers.clear();
    m_memory_barriers.clear();
}

}  // namespace lr::Graphics
