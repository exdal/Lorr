#include "MemoryAllocator.hh"

#include "Device.hh"

namespace lr::Graphics
{

void MemoryAllocator::init(Device *device)
{
    m_device = device;
}

LinearMemoryPool *MemoryAllocator::create_linear_memory_pool(MemoryPoolLocation location, usize initial_capacity)
{
    ZoneScoped;

    auto pool_id = set_handle_val<PoolID>(m_linear_pools.size());
    auto &pool = m_linear_pools.push_back();

    pool.m_id = pool_id;
    pool.m_location = location;
    grow_pool(pool_id, initial_capacity);

    return &pool;
}

MemoryFlag MemoryAllocator::get_preferred_memory_flag(MemoryPoolLocation location, usize size)
{
    ZoneScoped;

    switch (location)
    {
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
void MemoryAllocator::grow_pool(PoolID pool_id, usize size)
{
    ZoneScoped;

    auto &pool = m_linear_pools[get_handle_val(pool_id)];
    MemoryFlag target_heap = get_preferred_memory_flag(pool.m_location, size);

    auto &memory = pool.m_memories.push_back();
    if (m_device->is_feature_supported(DeviceFeature::MemoryBudget))
        size = eastl::min(size, m_device->get_heap_budget(target_heap));

    DeviceMemoryDesc device_memory_desc = {
        .m_flags = target_heap,
        .m_size = size,
    };
    m_device->create_device_memory(&memory, &device_memory_desc);
    pool.m_ranges.push_back({ 0, size });
}
}  // namespace lr::Graphics