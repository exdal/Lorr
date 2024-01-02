#pragma once

#include "PhysicalDevice.hh"

namespace lr::Graphics
{
enum class MemoryPoolLocation
{
    GPU = 0,
    CPU,
    PreferGPU,
    PreferCPU,
    Count,
};

LR_HANDLE(PoolID, usize);
struct LinearMemoryPool
{
    struct MemoryRange
    {
        u64 offset = 0;
        u64 capacity = 0;
    };

    PoolID m_id = PoolID::Invalid;
    MemoryPoolLocation m_location = MemoryPoolLocation::Count;
    eastl::vector<MemoryRange> m_ranges = {};
    eastl::vector<DeviceMemory> m_memories = {};
};

struct MemoryAllocator
{
    void init(Device *device);
    LinearMemoryPool *create_linear_memory_pool(MemoryPoolLocation location, usize initial_capacity);

    MemoryFlag get_preferred_memory_flag(MemoryPoolLocation location, usize size);
    void grow_pool(PoolID pool_id, usize size);

    eastl::vector<LinearMemoryPool> m_linear_pools = {};
    Device *m_device = nullptr;
};
}  // namespace lr::Graphics