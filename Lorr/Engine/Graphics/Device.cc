// Created on Monday March 20th 2023 by exdal
// Last modified on Wednesday August 2nd 2023 by exdal

#include "Device.hh"

#include "Window/Win32/Win32Window.hh"

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

void PhysicalDevice::Init(eastl::span<VkPhysicalDevice> devices)
{
    ZoneScoped;

    /// FIND SUITABLE DEVICE ///

    auto IsSuitable = [](eastl::span<VkPhysicalDevice> devices,
                         VkPhysicalDeviceType type,
                         bool &featuresSupported) -> VkPhysicalDevice
    {
        for (VkPhysicalDevice pDevice : devices)
        {
            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(pDevice, &properties);

            VkPhysicalDeviceFeatures features = {};
            vkGetPhysicalDeviceFeatures(pDevice, &features);

            if (properties.deviceType == type)
                return pDevice;
        }

        return VK_NULL_HANDLE;
    };

    bool featureSupport = false;
    VkPhysicalDevice pDevice = VK_NULL_HANDLE;

    pDevice = IsSuitable(devices, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, featureSupport);
    if (!pDevice)
        pDevice = IsSuitable(devices, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, featureSupport);
    if (!pDevice)
        pDevice = IsSuitable(devices, VK_PHYSICAL_DEVICE_TYPE_CPU, featureSupport);
    if (!pDevice)
        LOG_CRITICAL("What?");

    m_pHandle = pDevice;

    /// GET AVAILABLE DEVICE EXTENSIONS ///

    u32 deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_pHandle, nullptr, &deviceExtensionCount, nullptr);

    m_DeviceExtensions.resize(deviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(m_pHandle, nullptr, &deviceExtensionCount, m_DeviceExtensions.data());

    LOG_INFO("Checking device extensions:");
    for (eastl::string_view required : kRequiredExtensions)
    {
        bool foundExtension = false;
        for (VkExtensionProperties &properties : m_DeviceExtensions)
        {
            if (required != properties.extensionName)
                continue;

            foundExtension = true;
        }

        LOG_INFO("\t{}, {}", required, foundExtension);

        assert(foundExtension);
    }

    /// GET DEVICE FEATURES/INFOS ///

    m_FeatureDescriptorBufferProps = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
    };

    VkPhysicalDeviceProperties2 deviceProps2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &m_FeatureDescriptorBufferProps,
    };
    vkGetPhysicalDeviceProperties2(m_pHandle, &deviceProps2);
    m_FeatureDeviceProps = deviceProps2.properties;

    vkGetPhysicalDeviceMemoryProperties(m_pHandle, &m_FeatureMemoryProps);
}

void PhysicalDevice::InitQueueFamilies(Surface *pSurface)
{
    ZoneScoped;

    u32 familyProperyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_pHandle, &familyProperyCount, nullptr);

    m_QueueProperties.resize(familyProperyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_pHandle, &familyProperyCount, m_QueueProperties.data());

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

VkDevice PhysicalDevice::GetLogicalDevice()
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

    VkDevice pDevice = nullptr;
    VkResult result = vkCreateDevice(m_pHandle, &deviceCreateInfo, nullptr, &pDevice);

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

u32 PhysicalDevice::SelectQueue(
    Surface *pSurface, VkQueueFlags desiredQueue, bool requirePresent, bool selectBest)
{
    ZoneScoped;

    constexpr VkQueueFlags flagsAll = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    u32 foundIdx = VK_QUEUE_FAMILY_IGNORED;
    u32 bestCount = ~0;

    for (u32 i = 0; i < m_QueueProperties.size(); i++)
    {
        VkQueueFamilyProperties &properties = m_QueueProperties[i];
        VkQueueFlags queueFlags = properties.queueFlags & flagsAll;  // filter other/not available flags

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
        vkGetPhysicalDeviceSurfaceSupportKHR(m_pHandle, foundIdx, pSurface->m_pHandle, &presentSupported);

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

void Surface::Init(BaseWindow *pWindow, VkInstance pInstance, PhysicalDevice *pPhysicalDevice)
{
    ZoneScoped;

    Win32Window *pOSWindow = (Win32Window *)pWindow;
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = (HINSTANCE)pOSWindow->m_pInstance,
        .hwnd = (HWND)pOSWindow->m_pHandle,
    };
    vkCreateWin32SurfaceKHR(pInstance, &surfaceInfo, nullptr, &m_pHandle);

    VkPhysicalDevice pDeviceHandle = pPhysicalDevice->m_pHandle;

    /// GET SUPPORTED SURFACE FORMATS ///

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pDeviceHandle, m_pHandle, &formatCount, nullptr);

    m_SurfaceFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pDeviceHandle, m_pHandle, &formatCount, m_SurfaceFormats.data());

    /// GET SUPPORTED PRESENT MODES ///

    u32 presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(pDeviceHandle, m_pHandle, &presentCount, nullptr);

    m_PresentModes.resize(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(pDeviceHandle, m_pHandle, &presentCount, m_PresentModes.data());

    /// GET SURFACE CAPABILITIES ///

    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pDeviceHandle, m_pHandle, &capabilities);

    m_MinImageCount = capabilities.minImageCount;
    m_MaxImageCount = capabilities.maxImageCount;
    m_MinImageSize = { capabilities.minImageExtent.width, capabilities.minImageExtent.width };
    m_MaxImageSize = { capabilities.maxImageExtent.width, capabilities.maxImageExtent.width };
    m_SupportedTransforms = capabilities.supportedTransforms;
    m_CurrentTransform = capabilities.currentTransform;
    m_SupportedCompositeAlpha = capabilities.supportedCompositeAlpha;
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

// we are maybe over complicating stuff...
DeviceMemory *ResourceAllocators::GetDeviceMemory(ResourceAllocator allocator)
{
    ZoneScoped;

    static DeviceMemory *kResourceAllocatorsCrazyShit[] = {
        nullptr,             // None
        m_pDescriptor,       // Descriptor
        m_pBufferLinear,     // BufferLinear
        m_pBufferTLSF,       // BufferTLSF
        m_pBufferTLSFHost,   // BufferTLSF_Host
        m_pBufferFrametime,  // BufferFrametime
        m_pImageTLSF,        // ImageTLSF
        nullptr,             // Count
    };

    return kResourceAllocatorsCrazyShit[(u32)allocator];
}

}  // namespace lr::Graphics