#include "PhysicalDevice.hh"

#include <EASTL/scoped_array.h>

#include "Device.hh"

namespace lr::Graphics
{

/// These extensions are REQUIRED to be available on selected device.

static const char *k_required_extensions_str[] = {
    "VK_KHR_swapchain",
    "VK_EXT_descriptor_buffer",
    "VK_EXT_calibrated_timestamps",
};

static constexpr eastl::span<const char *> k_required_extensions(k_required_extensions_str);

////////////////////////////////////
/// Device Feature Configuration ///
////////////////////////////////////

// * Extensions
static VkPhysicalDeviceBufferDeviceAddressFeaturesEXT kBufferDeviceAddressFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT,
    .pNext = nullptr,
    .bufferDeviceAddress = true,
};

static VkPhysicalDeviceDescriptorBufferFeaturesEXT kDesciptorBufferFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
    .pNext = &kBufferDeviceAddressFeatures,
    .descriptorBuffer = true,
    .descriptorBufferImageLayoutIgnored = true,
    .descriptorBufferPushDescriptors = true,
};

// * Vulkan 1.3

static VkPhysicalDeviceVulkan13Features kDeviceFeatures_13 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &kDesciptorBufferFeatures,
    .synchronization2 = true,
    .dynamicRendering = true,
};

// * Vulkan 1.2

static VkPhysicalDeviceVulkan12Features kDeviceFeatures_12 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = &kDeviceFeatures_13,
    .descriptorIndexing = true,
    .shaderSampledImageArrayNonUniformIndexing = true,
    .shaderStorageBufferArrayNonUniformIndexing = true,
    .descriptorBindingUpdateUnusedWhilePending = true,
    .descriptorBindingPartiallyBound = true,
    .descriptorBindingVariableDescriptorCount = false,
    .runtimeDescriptorArray = true,
    .timelineSemaphore = true,
};

// * Vulkan 1.1

static VkPhysicalDeviceVulkan11Features kDeviceFeatures_11 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = &kDeviceFeatures_12,
};

// * Vulkan 1.0

static VkPhysicalDeviceFeatures kDeviceFeatures_10 = {
    .vertexPipelineStoresAndAtomics = true,
    .fragmentStoresAndAtomics = true,
    .shaderInt64 = true,
};

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

PhysicalDevicePropertySet::PhysicalDevicePropertySet()
{
    m_descriptor_buffer = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
        .pNext = nullptr,
    };

    m_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &m_descriptor_buffer,
    };
}

APIResult PhysicalDevice::init(VkPhysicalDevice physical_device, eastl::span<QueueFamilyInfo> queue_family_infos)
{
    m_handle = physical_device;
    m_queue_family_infos.assign(queue_family_infos.begin(), queue_family_infos.end());

    return APIResult::Success;
}

APIResult PhysicalDevice::get_logical_device(Device *device)
{
    ZoneScoped;

    constexpr static f32 top_priority = 1.0;

    eastl::vector<VkDeviceQueueCreateInfo> queue_create_infos = {};
    for (auto &queue_family_info : m_queue_family_infos)
    {
        queue_create_infos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queue_family_info.m_index,
            .queueCount = 1,
            .pQueuePriorities = &top_priority,
        });
    }

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &kDeviceFeatures_11,
        .queueCreateInfoCount = static_cast<u32>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = k_required_extensions.size(),
        .ppEnabledExtensionNames = k_required_extensions.data(),
        .pEnabledFeatures = &kDeviceFeatures_10,
    };

    VkDevice device_handle = nullptr;
    auto result = static_cast<APIResult>(vkCreateDevice(m_handle, &device_create_info, nullptr, &device_handle));
    if (result != APIResult::Success)
    {
        LOG_ERROR("Failed to create Device! {}", static_cast<u32>(result));
        return result;
    }

    if (!VK::LoadVulkanDevice(device_handle))
    {
        LOG_ERROR("Failed to initialize device!");
        return APIResult::ExtNotPresent;
    }

    device->init(device_handle, this);

    return APIResult::Success;
}

u32 PhysicalDevice::get_heap_index(VkMemoryPropertyFlags flags)
{
    ZoneScoped;

    for (u32 i = 0; i < m_property_set.m_memory.memoryTypeCount; i++)
    {
        if ((m_property_set.m_memory.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }

    LOG_ERROR("Memory type index is not found.");

    return -1;
}

u64 PhysicalDevice::get_descriptor_buffer_alignment()
{
    return m_property_set.m_descriptor_buffer.descriptorBufferOffsetAlignment;
}

u64 PhysicalDevice::get_descriptor_size(DescriptorType type)
{
    ZoneScoped;

    auto &props = m_property_set.m_descriptor_buffer;
    u64 sizeArray[] = {
        [(u32)DescriptorType::Sampler] = props.samplerDescriptorSize,
        [(u32)DescriptorType::SampledImage] = props.sampledImageDescriptorSize,
        [(u32)DescriptorType::UniformBuffer] = props.uniformBufferDescriptorSize,
        [(u32)DescriptorType::StorageImage] = props.storageImageDescriptorSize,
        [(u32)DescriptorType::StorageBuffer] = props.storageBufferDescriptorSize,
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

eastl::span<const char *> PhysicalDevice::get_extensions()
{
    return k_required_extensions;
}

}  // namespace lr::Graphics