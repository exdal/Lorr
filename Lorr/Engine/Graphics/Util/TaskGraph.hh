#pragma once

#include "Graphics/CommandList.hh"

namespace lr::graphics {
struct TaskGraph {
    ls::static_vector<CommandAllocator, 3> m_command_allocators;
};

}  // namespace lr::graphics
