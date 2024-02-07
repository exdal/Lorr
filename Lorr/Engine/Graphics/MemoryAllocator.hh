#pragma once

#include "Resource.hh"

namespace lr::Graphics {
struct DeviceMemoryDesc {
    MemoryFlag m_flags = MemoryFlag::Device;
    u64 m_size = 0;
};

using DeviceMemoryID = usize;
struct DeviceMemory {
    MemoryFlag m_flags = {};
    void *m_mapped_memory = nullptr;

    VkDeviceMemory m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(DeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY);

enum class MemoryPoolLocation {
    GPU = 0,
    CPU,
    PreferGPU,
    PreferCPU,
    Count,
};

LR_HANDLE(PoolID, usize);
struct LinearMemoryPool {
    struct MemoryRange {
        u64 offset = 0;
        u64 capacity = 0;
    };

    PoolID m_id = PoolID::Invalid;
    MemoryPoolLocation m_location = MemoryPoolLocation::Count;
    eastl::vector<MemoryRange> m_ranges = {};
    eastl::vector<DeviceMemory> m_memories = {};
    bool m_reconstruct = false;
};

struct Device;
struct MemoryAllocator {
    void init(Device *device);
    PoolID create_linear_memory_pool(MemoryPoolLocation location, usize initial_capacity);

    MemoryFlag get_preferred_memory_flag(MemoryPoolLocation location, usize size);
    bool can_allocate(PoolID pool_id, usize aligned_size);
    void grow_pool(PoolID pool_id, usize aligned_size);

    void reset(PoolID pool_id);
    void allocate(PoolID pool_id, Buffer *buffer, void *&host_memory);

    auto &get_linear_pool(PoolID pool_id) { return m_linear_pools[get_handle_val(pool_id)]; }

    eastl::vector<LinearMemoryPool> m_linear_pools = {};
    Device *m_device = nullptr;
};
}  // namespace lr::Graphics
