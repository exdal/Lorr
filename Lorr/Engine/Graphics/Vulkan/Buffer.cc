#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto Buffer::create(Device &device, u64 size, vuk::MemoryUsage memory_usage, vuk::source_location LOC)
    -> std::expected<Buffer, vuk::VkException> {
    ZoneScoped;

    vuk::BufferCreateInfo create_info = {
        .mem_usage = memory_usage,
        .size = size,
        .alignment = 8,
    };

    vuk::Unique<vuk::Buffer> buffer_handle(*device.allocator);
    auto result = device.allocator->allocate_buffers({ &*buffer_handle, 1 }, { &create_info, 1 }, LOC);
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto buffer = Buffer{};
    buffer.data_size_ = buffer_handle->size;
    buffer.host_data_ = buffer_handle->mapped_ptr;
    buffer.device_address_ = buffer_handle->device_address;
    buffer.id_ = device.resources.buffers.create_slot(std::move(buffer_handle));
    if (device.bda_array_buffer) {
        auto slot_index = SlotMap_decode_id(buffer.id_).index;
        auto *device_address_arr = reinterpret_cast<u64 *>(device.bda_array_buffer.host_ptr());
        device_address_arr[slot_index] = buffer_handle->device_address;
    }

    return buffer;
}

auto Buffer::data_size() const -> u64 {
    return data_size_;
}

auto Buffer::device_address() const -> u64 {
    return device_address_;
}

auto Buffer::host_ptr() const -> u8 * {
    return static_cast<u8 *>(host_data_);
}

auto Buffer::id() const -> BufferID {
    return id_;
}

}  // namespace lr
