#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto Buffer::create(Device &device, u64 size, vuk::MemoryUsage memory_usage, LR_CALLSTACK) -> std::expected<Buffer, vuk::VkException> {
    ZoneScoped;

    vuk::BufferCreateInfo create_info = {
        .mem_usage = memory_usage,
        .size = size,
        .alignment = 8,
    };

    auto buffer_handle = vuk::Buffer{};
    auto result = device.allocator->allocate_buffers({ &buffer_handle, 1 }, { &create_info, 1 }, LOC);
    if (!result.holds_value()) {
        return std::unexpected(result.error());
    }

    auto buffer = Buffer{};
    buffer.data_size_ = buffer_handle.size;
    buffer.host_data_ = buffer_handle.mapped_ptr;
    buffer.device_address_ = buffer_handle.device_address;
    buffer.id_ = device.resources.buffers.create_slot(static_cast<vuk::Buffer &&>(buffer_handle));

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

auto Buffer::acquire(Device &device, vuk::Name name, vuk::Access access, u64 offset, u64 size) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto vuk_handle = *device.buffer(this->id_);
    return vuk::acquire_buf(name, vuk_handle.subrange(offset, size != ~0_u64 ? size : this->data_size_), access);
}

auto Buffer::discard(Device &device, vuk::Name name, u64 offset, u64 size) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto vuk_handle = *device.buffer(this->id_);
    return vuk::discard_buf(name, vuk_handle.subrange(offset, size != ~0_u64 ? size : this->data_size_));
}

auto Buffer::subrange(Device &device, u64 offset, u64 size) -> vuk::Buffer {
    ZoneScoped;

    auto vuk_handle = *device.buffer(this->id_);
    return vuk_handle.subrange(offset, size != ~0_u64 ? size : this->data_size_);
}

} // namespace lr
