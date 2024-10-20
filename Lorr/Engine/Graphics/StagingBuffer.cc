#include "Engine/Graphics/StagingBuffer.hh"

#include "Engine/Graphics/Device.hh"

namespace lr {
StagingBuffer::StagingBuffer(Device *device_, usize capacity_)
    : device(device_) {
    ZoneScoped;

    this->device->create_timeline_semaphores(this->semaphore, 0);
    this->new_allocation(this->head_block, capacity_, true);
}

ls::option<StagingBuffer::Allocation> StagingBuffer::allocate(this StagingBuffer &self, u64 size, u64 alignment) {
    ZoneScoped;

    u32 offset_aligned = ls::align_up(self.offset, alignment);
    u32 offset_padding = offset_aligned - self.offset;
    if (offset_aligned + size > self.head_block.capacity) {
        AdditionalAllocation additional_allocation = {};
        self.new_allocation(additional_allocation, size);
        return Allocation{
            .ptr = additional_allocation.buffer_data,
            .offset = 0,
            .size = size,
        };
    }

    self.semaphore.advance();
    u32 return_offset = offset_aligned;
    u32 allocation_size = size + offset_padding;

    self.offset += allocation_size;
    return Allocation{
        .ptr = self.head_block.buffer_data + return_offset,
        .offset = return_offset,
        .size = allocation_size,
    };
}

void StagingBuffer::reset(this StagingBuffer &self) {
    ZoneScoped;

    AdditionalAllocation *largest_block = nullptr;
    for (auto &v : self.additional_allocations) {
        if (!largest_block || v.capacity > largest_block->capacity) {
            largest_block = &v;
        }
    }

    if (largest_block && largest_block->capacity > self.head_block.capacity) {
        std::swap(self.head_block, *largest_block);
    }

    for (auto &v : self.additional_allocations) {
        self.device->delete_buffers(v.buffer);
    }

    self.offset = 0;
    self.additional_allocations.clear();
}

void StagingBuffer::new_allocation(this StagingBuffer &self, AdditionalAllocation &allocation, u64 capacity, bool head) {
    ZoneScoped;

    allocation.buffer = self.device->create_buffer(BufferInfo{
        .flags = MemoryFlag::HostSeqWrite | MemoryFlag::Dedicated,
        .preference = MemoryPreference::Auto,
        .data_size = capacity,
    });
    allocation.buffer_data = self.device->buffer_host_data(allocation.buffer);
    allocation.capacity = capacity;

    if (!head) {
        self.additional_allocations.push_back(allocation);
    }
}

}  // namespace lr
