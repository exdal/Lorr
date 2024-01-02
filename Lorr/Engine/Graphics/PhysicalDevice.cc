#include "PhysicalDevice.hh"

#include "Device.hh"

namespace lr::Graphics
{
APIResult Surface::init(PhysicalDevice *physical_device)
{
    {
        u32 count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, m_handle, &count, nullptr);
        m_surface_formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, m_handle, &count, m_surface_formats.data());
    }
    {
        u32 count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device, m_handle, &count, nullptr);
        m_present_modes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(*physical_device, m_handle, &count, m_present_modes.data());
    }

    refresh_surface_capabilities(physical_device);

    return APIResult::Success;
}

void Surface::refresh_surface_capabilities(PhysicalDevice *physical_device)
{
    ZoneScoped;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physical_device, m_handle, &m_capabilities);
}

bool Surface::is_format_supported(VkFormat format, VkColorSpaceKHR &color_space_out)
{
    ZoneScoped;

    for (VkSurfaceFormatKHR &surfaceFormat : m_surface_formats)
    {
        if (format == surfaceFormat.format)
        {
            color_space_out = surfaceFormat.colorSpace;
            return true;
        }
    }

    return false;
}

bool Surface::is_present_mode_supported(VkPresentModeKHR mode)
{
    ZoneScoped;

    for (VkPresentModeKHR presentMode : m_present_modes)
        if (presentMode == mode)
            return true;

    return false;
}

VkSurfaceTransformFlagBitsKHR Surface::get_transform()
{
    ZoneScoped;

    return m_capabilities.currentTransform;
}

APIResult PhysicalDevice::init(VkPhysicalDevice handle, DeviceFeature features)
{
    m_handle = handle;
    m_supported_features = features;

    chained_struct cs(m_properties);
    cs.set_next(m_descriptor_buffer_props);

    vkGetPhysicalDeviceProperties2(m_handle, &m_properties);
    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_memory_props2);

    return APIResult::Success;
}

u32 PhysicalDevice::get_memory_type_index(MemoryFlag flags)
{
    ZoneScoped;

    auto vk_flags = (VkMemoryPropertyFlags)flags;
    auto &mem_props = m_memory_props2.memoryProperties;
    for (u32 i = 0; i < mem_props.memoryTypeCount; i++)
        if ((mem_props.memoryTypes[i].propertyFlags & vk_flags) == vk_flags)
            return i;

    LOG_ERROR("Memory type index is not found.");

    return -1;
}

u32 PhysicalDevice::get_heap_index(MemoryFlag flags)
{
    ZoneScoped;

    u32 i = get_memory_type_index(flags);
    return m_memory_props2.memoryProperties.memoryTypes[i].heapIndex;
}

u64 PhysicalDevice::get_heap_budget(MemoryFlag flags)
{
    ZoneScoped;

    if (!is_feature_supported(DeviceFeature::MemoryBudget))
        return ~0;

    m_memory_props2.pNext = &m_memory_budget;
    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_memory_props2);

    return m_memory_budget.heapBudget[get_heap_index(flags)];
}

u64 PhysicalDevice::get_heap_usage(MemoryFlag flags)
{
    ZoneScoped;

    if (!is_feature_supported(DeviceFeature::MemoryBudget))
        return ~0;

    m_memory_props2.pNext = &m_memory_budget;
    vkGetPhysicalDeviceMemoryProperties2(m_handle, &m_memory_props2);

    return m_memory_budget.heapUsage[get_heap_index(flags)];
}

u64 PhysicalDevice::get_descriptor_buffer_alignment()
{
    return m_descriptor_buffer_props.descriptorBufferOffsetAlignment;
}

u64 PhysicalDevice::get_descriptor_size(DescriptorType type)
{
    ZoneScoped;

    u64 sizeArray[] = {
        [(u32)DescriptorType::Sampler] = m_descriptor_buffer_props.samplerDescriptorSize,
        [(u32)DescriptorType::SampledImage] = m_descriptor_buffer_props.sampledImageDescriptorSize,
        [(u32)DescriptorType::UniformBuffer] = m_descriptor_buffer_props.uniformBufferDescriptorSize,
        [(u32)DescriptorType::StorageImage] = m_descriptor_buffer_props.storageImageDescriptorSize,
        [(u32)DescriptorType::StorageBuffer] = m_descriptor_buffer_props.storageBufferDescriptorSize,
    };

    return sizeArray[(u32)type];
}

u64 PhysicalDevice::get_aligned_buffer_memory(BufferUsage buffer_usage, u64 unaligned_size)
{
    constexpr static BufferUsage k_descriptors = BufferUsage::ResourceDescriptor | BufferUsage::SamplerDescriptor;

    u64 result = unaligned_size;
    if (buffer_usage & k_descriptors)
        result = Memory::align_up(result, get_descriptor_buffer_alignment());

    return result;
}

bool PhysicalDevice::is_feature_supported(DeviceFeature feature)
{
    return m_supported_features & feature;
}

}  // namespace lr::Graphics