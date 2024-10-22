#include "Engine/Graphics/Vulkan/Impl.hh"

#include "Engine/Graphics/Vulkan/ToVk.hh"

namespace lr {
auto Buffer::create(
    Device_H device, vk::BufferUsage usage_flags, u64 size, vk::MemoryAllocationUsage memory_usage, vk::MemoryAllocationFlag memory_flags)
    -> std::expected<BufferID, vk::Result> {
    ZoneScoped;

    constexpr static auto host_flags = vk::MemoryAllocationFlag::HostSeqWrite | vk::MemoryAllocationFlag::HostReadWrite;
    auto vma_creation_flags = to_vma_memory_flags(memory_flags);
    if (memory_flags & host_flags) {
        vma_creation_flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    auto buffer_usage_flags = to_vk_buffer_flags(usage_flags) | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = buffer_usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo allocation_info = {
        .flags = vma_creation_flags,
        .usage = to_vma_usage(memory_usage),
        .requiredFlags = 0,
        .preferredFlags = 0,
        .memoryTypeBits = ~0u,
        .pool = nullptr,
        .pUserData = nullptr,
        .priority = 1.0f,
    };

    VkBuffer buffer_handle = {};
    VmaAllocation allocation = {};
    VmaAllocationInfo allocation_result = {};
    auto result = vmaCreateBuffer(device->allocator, &create_info, &allocation_info, &buffer_handle, &allocation, &allocation_result);
    if (result != VK_SUCCESS) {
        return std::unexpected(vk::Result::Unknown);
    }

    VkBufferDeviceAddressInfo device_address_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer_handle,
    };

    auto buffer = device->resources.buffers.create();
    if (!buffer.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = buffer->impl;
    impl->device = device;
    impl->data_size = allocation_result.size;
    impl->host_data = allocation_result.pMappedData;
    impl->device_address = vkGetBufferDeviceAddress(device->handle, &device_address_info);
    impl->allocation = allocation;
    impl->handle = buffer_handle;

    if (device->bda_array_host_addr) {
        device->bda_array_host_addr[static_cast<usize>(buffer->id)] = impl->device_address;
    }

    return buffer->id;
}

auto Buffer::destroy() -> void {
    vmaDestroyBuffer(impl->device->allocator, impl->handle, impl->allocation);
}

auto Buffer::set_name(const std::string &name) -> Buffer & {
    vmaSetAllocationName(impl->device->allocator, impl->allocation, name.c_str());
    set_object_name(impl->device, impl->handle, VK_OBJECT_TYPE_BUFFER, name);

    return *this;
}

auto Buffer::data_size() -> u64 {
    return impl->data_size;
}

auto Buffer::device_address() -> u64 {
    return impl->device_address;
}

auto Buffer::host_ptr() -> u8 * {
    return static_cast<u8 *>(impl->host_data);
}

}  // namespace lr
