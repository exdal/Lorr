#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct StagingBuffer {
    struct Allocation {
        BufferID buffer = BufferID::Invalid;
        u8 *ptr = nullptr;
        u64 offset = 0;
        u64 size = 0;
    };

    struct AdditionalAllocation {
        u8 *buffer_data = nullptr;
        BufferID buffer = BufferID::Invalid;
        u32 capacity = 0;
    };

    Device *device = nullptr;
    Semaphore semaphore = {};
    u32 offset = 0;
    AdditionalAllocation head_block = {};

    // Additional separate allocations to prevent throttling
    std::vector<AdditionalAllocation> additional_allocations = {};

    StagingBuffer() = default;
    StagingBuffer(Device *device_, usize capacity_);

    ls::option<Allocation> allocate(this StagingBuffer &, u64 size, u64 alignment = 16);
    // When called, this function will select largest block existing
    // in `additional_allocations` and will swap it with `head_block`.
    void reset(this StagingBuffer &);

    void new_allocation(this StagingBuffer &, AdditionalAllocation &allocation, u64 capacity, bool head = false);
};

}  // namespace lr
