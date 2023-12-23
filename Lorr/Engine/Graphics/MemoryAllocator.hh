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

using ResourceID = u64;
constexpr static ResourceID resource_null = ~0;

template<typename ResourceT>
struct ResourcePool
{
    static constexpr u64 FL_SIZE = 63;
    static constexpr u64 SL_SIZE = 63;
    static constexpr u64 SL_SHIFT = static_cast<u64>(1u) << SL_SIZE;
    static constexpr u64 SL_MASK = SL_SHIFT - 1u;
    static constexpr u64 RESOURCE_COUNT = FL_SIZE * SL_SIZE;

    ResourcePool() { m_resources.resize(RESOURCE_COUNT); }

    ResourceID add_resource()
    {
        ZoneScoped;

        if (m_first_list == eastl::numeric_limits<decltype(m_first_list)>::max())
            return resource_null;

        u64 first_index = Memory::find_lsb(~m_first_list);
        u64 second_list = m_second_list[first_index];
        u64 second_index = Memory::find_lsb(~second_list);

        second_list |= 1u << second_index;
        if (_popcnt64(second_list) == SL_SIZE)
            m_first_list |= 1u << first_index;

        m_second_list[first_index] = second_list;
        return get_index(first_index, second_index);
    }

    void remove_resource(ResourceID id)
    {
        ZoneScoped;

        u64 first_index = id >> SL_SIZE;
        u64 second_index = id & SL_MASK;
        u64 second_list = m_second_list[first_index];

        second_list &= ~(1 << second_index);
        if (_popcnt64(second_list) != SL_SIZE)
            m_first_list &= ~(1 << first_index);

        m_second_list[first_index] = second_list;
    }

    bool validate_id(ResourceID id)
    {
        ZoneScoped;

        if (id == resource_null)
            return false;

        u64 first_index = id >> SL_SIZE;
        u64 second_index = id & SL_MASK;
        u64 second_list = m_second_list[first_index];

        return (second_list >> second_index) & 0x1;
    }

    static u64 get_index(u64 fi, u64 si) { return fi == 0 ? si : fi * SL_SIZE + si; }
    ResourceT &get_resource(ResourceID id) const { return m_resources[id]; }

    u64 m_first_list = 0;  // This bitmap indicates that SL[bit(i)] is full or not
    u64 m_second_list[FL_SIZE] = {};
    eastl::vector<ResourceT> m_resources = {};
};

}  // namespace lr::Graphics