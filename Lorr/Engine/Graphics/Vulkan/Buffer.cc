#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto Buffer::create(Device &device, u64 size, vuk::MemoryUsage memory_usage) -> std::expected<Buffer, vuk::VkException> {
    ZoneScoped;

    vuk::BufferCreateInfo create_info = {
        .mem_usage = memory_usage,
        .size = size,
        .alignment = 8,
    };

    vuk::Buffer buffer_handle = {};
    auto result = device.allocator->allocate_buffers({ &buffer_handle, 1 }, { &create_info, 1 });
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto buffer_resource = device.resources.buffers.create();
    *buffer_resource->self = buffer_handle;

    if (device.bda_array_buffer) {
        auto *device_address_arr = reinterpret_cast<u64 *>(device.bda_array_buffer.host_ptr());
        device_address_arr[static_cast<usize>(buffer_resource->id)] = buffer_handle.device_address;
    }

    auto buffer = Buffer{};
    buffer.data_size_ = buffer_handle.size;
    buffer.host_data_ = buffer_handle.mapped_ptr;
    buffer.device_address_ = buffer_handle.device_address;
    buffer.id_ = buffer_resource->id;
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
