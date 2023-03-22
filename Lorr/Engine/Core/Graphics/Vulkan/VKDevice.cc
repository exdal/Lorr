#include "VKDevice.hh"

#include "Core/Window/Win32/Win32Window.hh"

#include "VKContext.hh"

namespace lr::Graphics
{

/// These extensions are REQUIRED to be available on selected device.

static constexpr eastl::array<const char *, 6> kRequiredExtensions = {
    "VK_KHR_swapchain",        "VK_KHR_depth_stencil_resolve",   "VK_KHR_dynamic_rendering",
    "VK_KHR_synchronization2", "VK_EXT_extended_dynamic_state2", "VK_KHR_timeline_semaphore",
};

////////////////////////////////////
/// Device Feature Configuration ///
////////////////////////////////////

// * Vulkan 1.3

static VkPhysicalDeviceVulkan13Features kDeviceFeatures_13 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = nullptr,
    .synchronization2 = true,
    .dynamicRendering = true,
};

// * Vulkan 1.2

static VkPhysicalDeviceVulkan12Features kDeviceFeatures_12 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = &kDeviceFeatures_13,
    .timelineSemaphore = true,
};

// * Vulkan 1.1

static VkPhysicalDeviceVulkan11Features kDeviceFeatures_11 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = &kDeviceFeatures_12,
};

void VKPhysicalDevice::Init(eastl::span<VkPhysicalDevice> devices)
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

    for (eastl::string_view required : kRequiredExtensions)
    {
        LOG_INFO("\tChecking for extension {}...", required);
        for (VkExtensionProperties &properties : m_DeviceExtensions)
            if (required == properties.extensionName)
                continue;

        LOG_ERROR("Following extension is not found in this device: {}", required);
        return;
    }

    /// GET DEVICE MEMORY INFO ///

    vkGetPhysicalDeviceMemoryProperties(m_pHandle, &m_MemoryInfo);
}

void VKPhysicalDevice::InitQueueFamilies(VKSurface *pSurface)
{
    ZoneScoped;

    auto TestQueue = [this, &pSurface](VkQueueFlags desiredQueue, bool requirePresent) -> bool
    {
        constexpr VkQueueFlags flagsAll = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
        for (u32 i = 0; i < m_QueueProperties.size(); i++)
        {
            VkQueueFamilyProperties &properties = m_QueueProperties[i];
            VkQueueFlags queueFlags = properties.queueFlags & flagsAll;  // filter other/not available flags

            if (!(queueFlags & desiredQueue))
                continue;

            VkBool32 presentSupported = false;
            if (requirePresent)
            {
                vkGetPhysicalDeviceSurfaceSupportKHR(m_pHandle, i, pSurface->m_pHandle, &presentSupported);

                if (!presentSupported)
                {
                    LOG_ERROR("Present mode not supported on queue {}!", i);
                    return false;
                }

                m_PresentQueueIndex = i;
            }

            return true;
        }

        return false;
    };

    u32 familyProperyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_pHandle, &familyProperyCount, nullptr);

    m_QueueProperties.resize(familyProperyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_pHandle, &familyProperyCount, m_QueueProperties.data());

    TestQueue(VK_QUEUE_GRAPHICS_BIT, true);
    TestQueue(VK_QUEUE_COMPUTE_BIT, false);
    TestQueue(VK_QUEUE_TRANSFER_BIT, false);
}

VkDevice VKPhysicalDevice::GetLogicalDevice()
{
    ZoneScoped;

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &kDeviceFeatures_11,
        .queueCreateInfoCount = m_QueueInfo.count,
        .pQueueCreateInfos = m_QueueInfo.data(),
        .enabledExtensionCount = kRequiredExtensions.count,
        .ppEnabledExtensionNames = kRequiredExtensions.data(),
    };

    VkDevice pDevice = nullptr;
    VkResult result = vkCreateDevice(m_pHandle, &deviceCreateInfo, nullptr, &pDevice);

    return pDevice;
}

u32 VKPhysicalDevice::GetHeapIndex(VkMemoryPropertyFlags flags)
{
    ZoneScoped;

    for (u32 i = 0; i < m_MemoryInfo.memoryTypeCount; i++)
    {
        if ((m_MemoryInfo.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    }

    LOG_ERROR("Memory type index is not found.");

    return -1;
}

u32 VKPhysicalDevice::GetQueueIndex(CommandListType type)
{
    ZoneScoped;

    return m_QueueInfo[type].queueFamilyIndex;
}

void VKSurface::Init(BaseWindow *pWindow, VkInstance pInstance, VKPhysicalDevice *pPhysicalDevice)
{
    ZoneScoped;

    Win32Window *pOSWindow = (Win32Window *)pWindow;
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = pOSWindow->m_Instance,
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
}  // namespace lr::Graphics

bool VKSurface::IsFormatSupported(VkFormat format, VkColorSpaceKHR &colorSpaceOut)
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

bool VKSurface::IsPresentModeSupported(VkPresentModeKHR mode)
{
    ZoneScoped;

    for (VkPresentModeKHR presentMode : m_PresentModes)
        if (presentMode == mode)
            return true;

    return false;
}

}  // namespace lr::Graphics