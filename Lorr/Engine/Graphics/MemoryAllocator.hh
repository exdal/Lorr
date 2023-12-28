#pragma once

#include "PhysicalDevice.hh"

namespace lr::Graphics
{
enum class MemoryPoolLocation
{
    GPU = 0,
    CPU,
    Count,
};

using DeviceMemoryID = usize;
using MemoryPoolID = usize;
struct MemoryPool
{
    MemoryPoolLocation m_location = MemoryPoolLocation::Count;
    eastl::vector<DeviceMemoryID> m_memories = {};
};

struct MemoryAllocator
{
    void init(Device *device);

    eastl::vector<MemoryPool> m_pools = {};
    eastl::vector<DeviceMemory *> m_device_memories = {};

    Device *m_device = nullptr;
};
}  // namespace lr::Graphics