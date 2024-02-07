#include "MemoryAllocator.hh"

#include "Memory/MemoryUtils.hh"

#include "Device.hh"

namespace lr::Graphics {

void MemoryAllocator::init(Device *device)
{
    m_device = device;
}

PoolID MemoryAllocator::create_linear_memory_pool(MemoryPoolLocation location, usize initial_capacity)
{
    ZoneScoped;

    auto pool_id = set_handle_val<PoolID>(m_linear_pools.size());
    auto &pool = m_linear_pools.push_back();

    pool.m_id = pool_id;
    pool.m_location = location;
    grow_pool(pool_id, initial_capacity);

    return pool_id;
}

MemoryFlag MemoryAllocator::get_preferred_memory_flag(MemoryPoolLocation location, usize size)
{
    ZoneScoped;

    switch (location) {
        case MemoryPoolLocation::GPU:
            return MemoryFlag::Device;
        case MemoryPoolLocation::CPU:
            return MemoryFlag::HostVisibleCoherent;
        case MemoryPoolLocation::PreferGPU:
            if (m_device->is_feature_supported(DeviceFeature::MemoryBudget) && m_device->get_heap_budget(MemoryFlag::Device) > size)
                return MemoryFlag::Device;

            return MemoryFlag::HostVisibleCoherent;
        case MemoryPoolLocation::PreferCPU:
            if (m_device->is_feature_supported(DeviceFeature::MemoryBudget) && m_device->get_heap_budget(MemoryFlag::HostVisibleCoherent) > size)
                return MemoryFlag::HostVisibleCoherent;

            return MemoryFlag::Device;
        case MemoryPoolLocation::Count:
            break;
    }

    return {};
}

bool MemoryAllocator::can_allocate(PoolID pool_id, usize aligned_size)
{
    ZoneScoped;

    LinearMemoryPool &pool = get_linear_pool(pool_id);
    auto &last_range = pool.m_ranges.back();
    usize avail_space = last_range.capacity - last_range.offset;

    return avail_space >= aligned_size;
}

void MemoryAllocator::grow_pool(PoolID pool_id, usize aligned_size)
{
    ZoneScoped;

    LinearMemoryPool &pool = get_linear_pool(pool_id);
    MemoryFlag target_heap = get_preferred_memory_flag(pool.m_location, aligned_size);

    auto &memory = pool.m_memories.push_back();
    if (m_device->is_feature_supported(DeviceFeature::MemoryBudget))
        aligned_size = eastl::min(aligned_size, m_device->get_heap_budget(target_heap));

    DeviceMemoryDesc device_memory_desc = {
        .m_flags = target_heap,
        .m_size = aligned_size,
    };
    m_device->create_device_memory(&memory, &device_memory_desc);
    pool.m_ranges.push_back({ 0, aligned_size });
    pool.m_reconstruct = true;
}

void MemoryAllocator::reset(PoolID pool_id)
{
    ZoneScoped;

    LinearMemoryPool &pool = get_linear_pool(pool_id);
    if (pool.m_ranges.empty())
        return;

    u64 new_size = 0;
    for (auto &range : pool.m_ranges) {
        range.offset = 0;
        new_size += range.capacity;
    }

    if (!pool.m_reconstruct)
        return;

    LOG_TRACE("Reconstructing linear allocator to size {} --- (memory count: {})", new_size, pool.m_ranges.size());

    for (DeviceMemory &mem : pool.m_memories)
        m_device->delete_device_memory(&mem);

    pool.m_ranges.clear();
    pool.m_memories.clear();

    grow_pool(pool_id, new_size);
    pool.m_reconstruct = false;
}

void MemoryAllocator::allocate(PoolID pool_id, Buffer *buffer, void *&host_memory)
{
    ZoneScoped;

    auto [mem_size, mem_alignment] = m_device->get_buffer_memory_size(buffer);
    u64 aligned_size = Memory::align_up(mem_size, mem_alignment);
    assert(aligned_size == mem_size);  // i am still not sure if mem_size == aligned_size

    if (!can_allocate(pool_id, aligned_size))
        grow_pool(pool_id, aligned_size);

    LinearMemoryPool &pool = get_linear_pool(pool_id);
    DeviceMemory &mem = pool.m_memories.back();
    auto &range = pool.m_ranges.back();

    buffer->m_pool_id = get_handle_val(pool_id);
    buffer->m_memory_id = pool.m_memories.size() - 1;

    host_memory = (void *)((u8 *)mem.m_mapped_memory + range.offset);
    m_device->bind_memory_ex(&mem, buffer, range.offset);

    range.offset += aligned_size;
}

}  // namespace lr::Graphics
