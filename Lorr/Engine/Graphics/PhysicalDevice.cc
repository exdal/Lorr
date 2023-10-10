#include "PhysicalDevice.hh"

#include "Window/Win32/Win32Window.hh"

#include "Device.hh"

namespace lr::Graphics
{

/// These extensions are REQUIRED to be available on selected device.

static const char *kppRequiredExtensions[] = {
    "VK_KHR_swapchain",
    "VK_KHR_depth_stencil_resolve",
    "VK_KHR_dynamic_rendering",
    "VK_KHR_synchronization2",
    "VK_EXT_extended_dynamic_state2",
    "VK_KHR_timeline_semaphore",
    "VK_EXT_descriptor_buffer",
    "VK_EXT_descriptor_indexing",
    "VK_EXT_host_query_reset",
    "VK_EXT_calibrated_timestamps",
};

static constexpr eastl::span<const char *> kRequiredExtensions(kppRequiredExtensions);

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

bool Surface::Init(VkSurfaceKHR pHandle)
{
    ZoneScoped;

    m_pHandle = pHandle;

    return true;
}

bool Surface::IsFormatSupported(VkFormat format, VkColorSpaceKHR &colorSpaceOut)
{
    ZoneScoped;

    for (VkSurfaceFormatKHR &surfaceFormat : m_SurfaceFormats)
    {
        if (format == surfaceFormat.format)
        {
            colorSpaceOut = surfaceFormat.colorSpace;
            return true;
        }
    }

    return false;
}

bool Surface::IsPresentModeSupported(VkPresentModeKHR mode)
{
    ZoneScoped;

    for (VkPresentModeKHR presentMode : m_PresentModes)
        if (presentMode == mode)
            return true;

    return false;
}

VkSurfaceTransformFlagBitsKHR Surface::GetTransform()
{
    ZoneScoped;

    return m_Capabilities.currentTransform;
}

bool PhysicalDevice::Init(VkPhysicalDevice pDevice)
{
    ZoneScoped;

    LOG_TRACE("Initializing physical device...");

    m_pHandle = pDevice;

    u32 availExtensions = 0;
    vkEnumerateDeviceExtensionProperties(m_pHandle, nullptr, &availExtensions, nullptr);
    VkExtensionProperties *pExtensions = new VkExtensionProperties[availExtensions];
    vkEnumerateDeviceExtensionProperties(
        m_pHandle, nullptr, &availExtensions, pExtensions);

    LOG_INFO("Checking device extensions:");
    for (eastl::string_view extension : kRequiredExtensions)
    {
        bool found = false;
        for (u32 i = 0; i < availExtensions; i++)
        {
            VkExtensionProperties &properties = pExtensions[i];
            if (properties.extensionName == extension)
            {
                found = true;
                break;
            }
        }

        LOG_INFO("\t{}, {}", extension, found);

        if (found)
            continue;

        LOG_ERROR("Following extension is not found in this device: {}", extension);

        return false;
    }

    delete[] pExtensions;

    m_FeatureDescriptorBufferProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
        .pNext = nullptr,
    };

    VkPhysicalDeviceProperties2 deviceProps2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &m_FeatureDescriptorBufferProps,
    };
    vkGetPhysicalDeviceProperties2(m_pHandle, &deviceProps2);
    m_FeatureDeviceProps = deviceProps2.properties;

    vkGetPhysicalDeviceMemoryProperties(m_pHandle, &m_FeatureMemoryProps);

    LOG_TRACE("Initialized physical device!");

    return true;
}

void PhysicalDevice::InitQueueFamilies(Surface *pSurface)
{
    ZoneScoped;

    u32 familyProperyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_pHandle, &familyProperyCount, nullptr);

    m_QueueProperties.resize(familyProperyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_pHandle, &familyProperyCount, m_QueueProperties.data());

    static float topPrior = 1.0;
    u32 queueIdx = SelectQueue(pSurface, VK_QUEUE_GRAPHICS_BIT, true, true);
    if (queueIdx != VK_QUEUE_FAMILY_IGNORED)
    {
        m_QueueInfos[(u32)CommandType::Graphics] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .queueFamilyIndex = queueIdx,
            .queueCount = 1,
            .pQueuePriorities = &topPrior,
        };
    }

    queueIdx = SelectQueue(pSurface, VK_QUEUE_COMPUTE_BIT, false, true);
    if (queueIdx != VK_QUEUE_FAMILY_IGNORED)
    {
        m_QueueInfos[(u32)CommandType::Compute] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .queueFamilyIndex = queueIdx,
            .queueCount = 1,
            .pQueuePriorities = &topPrior,
        };
    }

    queueIdx = SelectQueue(pSurface, VK_QUEUE_TRANSFER_BIT, false, true);
    if (queueIdx != VK_QUEUE_FAMILY_IGNORED)
    {
        m_QueueInfos[(u32)CommandType::Transfer] = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .queueFamilyIndex = queueIdx,
            .queueCount = 1,
            .pQueuePriorities = &topPrior,
        };
    }
}

Device *PhysicalDevice::GetLogicalDevice()
{
    ZoneScoped;

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &kDeviceFeatures_11,
        .queueCreateInfoCount = (u32)m_QueueInfos.size(),
        .pQueueCreateInfos = m_QueueInfos.data(),
        .enabledExtensionCount = kRequiredExtensions.size(),
        .ppEnabledExtensionNames = kRequiredExtensions.data(),
        .pEnabledFeatures = &kDeviceFeatures_10,
    };

    VkDevice pDeviceHandle = nullptr;
    VkResult result =
        vkCreateDevice(m_pHandle, &deviceCreateInfo, nullptr, &pDeviceHandle);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("Failed to create Device! {}", (u32)result);
        return nullptr;
    }

    Device *pDevice = new Device;
    if (!pDevice->Init(this, pDeviceHandle))
    {
        delete pDevice;
        return nullptr;
    }

    return pDevice;
}

u32 PhysicalDevice::GetHeapIndex(VkMemoryPropertyFlags flags)
{
    ZoneScoped;

    for (u32 i = 0; i < m_FeatureMemoryProps.memoryTypeCount; i++)
    {
        if ((m_FeatureMemoryProps.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }

    LOG_ERROR("Memory type index is not found.");

    return -1;
}

u32 PhysicalDevice::GetQueueIndex(CommandType type)
{
    ZoneScoped;

    return m_QueueInfos[(u32)type].queueFamilyIndex;
}

u64 PhysicalDevice::GetDescriptorBufferAlignment()
{
    return m_FeatureDescriptorBufferProps.descriptorBufferOffsetAlignment;
}

u64 PhysicalDevice::GetDescriptorSize(DescriptorType type)
{
    ZoneScoped;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT &props = m_FeatureDescriptorBufferProps;
    u64 sizeArray[] = {
        [(u32)DescriptorType::Sampler] = props.samplerDescriptorSize,
        [(u32)DescriptorType::SampledImage] = props.sampledImageDescriptorSize,
        [(u32)DescriptorType::UniformBuffer] = props.uniformBufferDescriptorSize,
        [(u32)DescriptorType::StorageImage] = props.storageImageDescriptorSize,
        [(u32)DescriptorType::StorageBuffer] = props.storageBufferDescriptorSize,
    };

    return sizeArray[(u32)type];
}

void PhysicalDevice::GetSurfaceFormats(
    Surface *pSurface, eastl::vector<VkSurfaceFormatKHR> &formats)
{
    ZoneScoped;

    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_pHandle, pSurface->m_pHandle, &count, nullptr);
    formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        m_pHandle, pSurface->m_pHandle, &count, formats.data());
}

void PhysicalDevice::GetPresentModes(
    Surface *pSurface, eastl::vector<VkPresentModeKHR> &modes)
{
    ZoneScoped;

    u32 count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_pHandle, pSurface->m_pHandle, &count, nullptr);
    modes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_pHandle, pSurface->m_pHandle, &count, modes.data());
}

void PhysicalDevice::SetSurfaceCapabilities(Surface *pSurface)
{
    ZoneScoped;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        m_pHandle, pSurface->m_pHandle, &pSurface->m_Capabilities);
}

u32 PhysicalDevice::SelectQueue(
    Surface *pSurface, VkQueueFlags desiredQueue, bool requirePresent, bool selectBest)
{
    ZoneScoped;

    constexpr VkQueueFlags flagsAll =
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    u32 foundIdx = VK_QUEUE_FAMILY_IGNORED;
    u32 bestCount = ~0;

    for (u32 i = 0; i < m_QueueProperties.size(); i++)
    {
        VkQueueFamilyProperties &properties = m_QueueProperties[i];
        VkQueueFlags queueFlags =
            properties.queueFlags & flagsAll;  // filter other/not available flags

        if (!(queueFlags & desiredQueue))
            continue;

        if (selectBest)
        {
            queueFlags ^= desiredQueue;
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

    if (requirePresent)
    {
        VkBool32 presentSupported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            m_pHandle, foundIdx, pSurface->m_pHandle, &presentSupported);

        if (!presentSupported)
        {
            LOG_ERROR("Present mode not supported on queue {}!", foundIdx);
            return VK_QUEUE_FAMILY_IGNORED;
        }

        m_PresentQueueIndex = foundIdx;
    }

    m_SelectedQueueIndices.push_back(foundIdx);

    return foundIdx;
}

u64 DeviceMemory::AlignBufferMemory(BufferUsage usage, u64 initialSize)
{
    ZoneScoped;

    constexpr static BufferUsage kDescriptors =
        BufferUsage::ResourceDescriptor | BufferUsage::SamplerDescriptor;

    u64 result = initialSize;
    if (usage & kDescriptors)
        result =
            Memory::AlignUp(result, m_pPhysicalDevice->GetDescriptorBufferAlignment());

    return result;
}

void TLSFDeviceMemory::Init(u64 memSize)
{
    ZoneScoped;

    m_Allocator.Init(memSize, 0x8000);
}

u64 TLSFDeviceMemory::Allocate(u64 dataSize, u64 alignment, u64 &allocatorData)
{
    ZoneScoped;

    Memory::TLSFBlockID blockID = m_Allocator.Allocate(dataSize, alignment);
    allocatorData = blockID;
    return m_Allocator.GetBlockData(blockID)->m_Offset;
}

void TLSFDeviceMemory::Free(u64 allocatorData)
{
    ZoneScoped;

    m_Allocator.Free(allocatorData);
}

void LinearDeviceMemory::Init(u64 memSize)
{
    ZoneScoped;

    m_AllocatorView.m_pFirstRegion = new Memory::AllocationRegion;
    m_AllocatorView.m_pFirstRegion->m_Capacity = memSize;
}

u64 LinearDeviceMemory::Allocate(u64 dataSize, u64 alignment, u64 &allocatorData)
{
    ZoneScoped;

    u64 alignedSize = Memory::AlignUp(dataSize, alignment);
    Memory::AllocationRegion *pAvailRegion = m_AllocatorView.FindFreeRegion(alignedSize);
    if (!pAvailRegion)
        return ~0;

    u64 offset = pAvailRegion->m_Size;
    pAvailRegion->m_Size += alignedSize;
    return offset;
}

void LinearDeviceMemory::Free(u64 allocatorData)
{
    ZoneScoped;

    m_AllocatorView.Reset();
}

}  // namespace lr::Graphics