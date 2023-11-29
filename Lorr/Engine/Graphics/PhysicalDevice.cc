#include "PhysicalDevice.hh"

#include <EASTL/scoped_array.h>

#include "Window/Win32/Win32Window.hh"

#include "Device.hh"

namespace lr::Graphics
{

/// These extensions are REQUIRED to be available on selected device.

static const char *kpp_required_extensions[] = {
    "VK_KHR_swapchain",                // Win32 or use DXGI
    "VK_KHR_depth_stencil_resolve",    // Core 1.2
    "VK_KHR_dynamic_rendering",        // Core 1.3
    "VK_KHR_synchronization2",         // Core 1.3
    "VK_EXT_extended_dynamic_state2",  // Core 1.3
    "VK_KHR_timeline_semaphore",       // Core 1.2
    "VK_EXT_descriptor_buffer",        //
    "VK_EXT_descriptor_indexing",      // Core 1.2
    "VK_EXT_host_query_reset",         // Core 1.2
    "VK_EXT_calibrated_timestamps",    //
};

static constexpr eastl::span<const char *> k_required_extensions(kpp_required_extensions);

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

Surface::Surface(VkSurfaceKHR handle)
    : m_handle(handle)
{
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

PhysicalDevice::PhysicalDevice(VkPhysicalDevice handle)
    : m_handle(handle)
{
    vkGetPhysicalDeviceProperties2(m_handle, &m_property_set.m_properties);
    vkGetPhysicalDeviceMemoryProperties(m_handle, &m_property_set.m_memory);
}

void PhysicalDevice::init_queue_families(Surface *surface)
{
    ZoneScoped;

    u32 familyProperyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &familyProperyCount, nullptr);

    m_queue_properties.resize(familyProperyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &familyProperyCount, m_queue_properties.data());

    static float topPrior = 1.0;
    u32 queueIdx = select_queue(surface, VK_QUEUE_GRAPHICS_BIT, true, true);
    if (queueIdx != VK_QUEUE_FAMILY_IGNORED)
    {
        m_queue_infos[(u32)CommandType::Graphics] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .queueFamilyIndex = queueIdx,
            .queueCount = 1,
            .pQueuePriorities = &topPrior,
        };
    }

    queueIdx = select_queue(surface, VK_QUEUE_COMPUTE_BIT, false, true);
    if (queueIdx != VK_QUEUE_FAMILY_IGNORED)
    {
        m_queue_infos[(u32)CommandType::Compute] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .queueFamilyIndex = queueIdx,
            .queueCount = 1,
            .pQueuePriorities = &topPrior,
        };
    }

    queueIdx = select_queue(surface, VK_QUEUE_TRANSFER_BIT, false, true);
    if (queueIdx != VK_QUEUE_FAMILY_IGNORED)
    {
        m_queue_infos[(u32)CommandType::Transfer] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .queueFamilyIndex = queueIdx,
            .queueCount = 1,
            .pQueuePriorities = &topPrior,
        };
    }
}

Device *PhysicalDevice::get_logical_device()
{
    ZoneScoped;

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &kDeviceFeatures_11,
        .queueCreateInfoCount = static_cast<u32>(m_queue_infos.size()),
        .pQueueCreateInfos = m_queue_infos.data(),
        .enabledExtensionCount = k_required_extensions.size(),
        .ppEnabledExtensionNames = k_required_extensions.data(),
        .pEnabledFeatures = &kDeviceFeatures_10,
    };

    VkDevice device_handle = nullptr;
    auto result = static_cast<APIResult>(vkCreateDevice(m_handle, &deviceCreateInfo, nullptr, &device_handle));
    if (result != APIResult::Success)
    {
        LOG_ERROR("Failed to create Device! {}", static_cast<u32>(result));
        return nullptr;
    }

    if (!VK::LoadVulkanDevice(device_handle))
    {
        LOG_ERROR("Failed to initialize device!");
        return nullptr;
    }

    return new Device(this, device_handle);
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

u32 PhysicalDevice::get_queue_index(CommandType type)
{
    ZoneScoped;

    return m_queue_infos[(u32)type].queueFamilyIndex;
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
        result = Memory::AlignUp(result, get_descriptor_buffer_alignment());

    return result;
}

void PhysicalDevice::get_surface_formats(Surface *surface, eastl::vector<VkSurfaceFormatKHR> &formats)
{
    ZoneScoped;

    formats.clear();

    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface->m_handle, &count, nullptr);
    formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_handle, surface->m_handle, &count, formats.data());
}

void PhysicalDevice::get_present_modes(Surface *surface, eastl::vector<VkPresentModeKHR> &modes)
{
    ZoneScoped;

    modes.clear();

    u32 count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface->m_handle, &count, nullptr);
    modes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_handle, surface->m_handle, &count, modes.data());
}

void PhysicalDevice::set_surface_capabilities(Surface *surface)
{
    ZoneScoped;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_handle, surface->m_handle, &surface->m_capabilities);
}

u32 PhysicalDevice::select_queue(Surface *surface, VkQueueFlags desired_queue, bool require_present, bool select_best)
{
    ZoneScoped;

    constexpr VkQueueFlags flagsAll = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    u32 foundIdx = VK_QUEUE_FAMILY_IGNORED;
    u32 bestCount = ~0;

    for (u32 i = 0; i < m_queue_properties.size(); i++)
    {
        VkQueueFamilyProperties &properties = m_queue_properties[i];
        VkQueueFlags queueFlags = properties.queueFlags & flagsAll;  // filter other/not available flags

        if (!(queueFlags & desired_queue))
            continue;

        if (select_best)
        {
            queueFlags ^= desired_queue;
            u32 setBitCount = __popcnt(queueFlags);
            if (setBitCount < bestCount)
            {
                bestCount = setBitCount;
                foundIdx = i;
            }
        }
        else
        {
            foundIdx = i;
            break;
        }
    }

    if (require_present)
    {
        VkBool32 presentSupported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, foundIdx, surface->m_handle, &presentSupported);

        if (!presentSupported)
        {
            LOG_ERROR("Present mode not supported on queue {}!", foundIdx);
            return VK_QUEUE_FAMILY_IGNORED;
        }

        m_present_queue_index = foundIdx;
    }

    return foundIdx;
}

void TLSFDeviceMemory::create(u64 mem_size, u32 max_allocations)
{
    ZoneScoped;

    m_allocator_view.init(mem_size, max_allocations);
}

u64 TLSFDeviceMemory::allocate_memory(u64 dataSize, u64 alignment, u64 &allocator_data)
{
    ZoneScoped;

    Memory::TLSFBlockID block_id = m_allocator_view.allocate(dataSize, alignment);
    allocator_data = block_id;
    return m_allocator_view.get_block_data(block_id)->m_offset;
}

void TLSFDeviceMemory::free_memory(u64 allocatorData)
{
    ZoneScoped;

    m_allocator_view.free(allocatorData);
}

void LinearDeviceMemory::create(u64 mem_size, u32 max_allocations)
{
    ZoneScoped;

    m_allocator_view.m_first_area = new Memory::AllocationArea;
    m_allocator_view.m_first_area->m_capacity = mem_size;
}

u64 LinearDeviceMemory::allocate_memory(u64 dataSize, u64 alignment, u64 &allocator_data)
{
    ZoneScoped;

    u64 alignedSize = Memory::AlignUp(dataSize, alignment);
    Memory::AllocationArea *pAvailRegion = m_allocator_view.find_free_area(alignedSize);
    if (!pAvailRegion)
        return ~0;

    u64 offset = pAvailRegion->m_size;
    pAvailRegion->m_size += alignedSize;
    return offset;
}

void LinearDeviceMemory::free_memory(u64 allocatorData)
{
    ZoneScoped;

    m_allocator_view.reset();
}

}  // namespace lr::Graphics