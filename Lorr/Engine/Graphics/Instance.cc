#include "Instance.hh"

#include <EASTL/scoped_array.h>

#include "Window/Win32/Win32Window.hh"

#include "PhysicalDevice.hh"
#include "Vulkan.hh"

namespace lr::Graphics
{
static const char *kppRequiredExtensions[] = {
    "VK_KHR_surface",
    "VK_KHR_get_physical_device_properties2",
    "VK_KHR_win32_surface",
#if LR_DEBUG
    //! Debug extensions, always put it to bottom
    "VK_EXT_debug_utils",
    "VK_EXT_debug_report",
#endif
};

static constexpr eastl::span<const char *> kRequiredExtensions(kppRequiredExtensions);

bool Instance::Init(InstanceDesc *pDesc)
{
    constexpr u32 kTypeMem = Memory::MiBToBytes(16);
    APIAllocator::g_Handle.m_TypeAllocator.Init(kTypeMem, 0x2000);
    APIAllocator::g_Handle.m_pTypeData = Memory::Allocate<u8>(kTypeMem);

    m_pVulkanLib = VK::LoadVulkan();
    if (!m_pVulkanLib)
    {
        LOG_ERROR("Failed to load Vulkan.");
        return false;
    }

    u32 availExtensions = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtensions, nullptr);
    eastl::scoped_array<VkExtensionProperties> pExtensions(
        new VkExtensionProperties[availExtensions]);
    vkEnumerateInstanceExtensionProperties(nullptr, &availExtensions, pExtensions.get());

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

        LOG_ERROR("Following extension is not found in this instance: {}", extension);

        return false;
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = pDesc->m_AppName.data(),
        .applicationVersion = pDesc->m_AppVersion,
        .pEngineName = pDesc->m_EngineName.data(),
        .engineVersion = pDesc->m_EngineVersion,
        .apiVersion = pDesc->m_APIVersion,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = (u32)kRequiredExtensions.size(),
        .ppEnabledExtensionNames = kRequiredExtensions.data(),
    };
    vkCreateInstance(&createInfo, nullptr, &m_pHandle);

    if (!VK::LoadVulkanInstance(m_pHandle))
        return false;

    return true;
}

PhysicalDevice *Instance::GetPhysicalDevice()
{
    ZoneScoped;

    u32 availDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_pHandle, &availDeviceCount, nullptr);

    if (availDeviceCount == 0)
    {
        LOG_ERROR("No GPU with Vulkan support.");
        return nullptr;
    }

    VkPhysicalDevice *ppAvailableDevices = new VkPhysicalDevice[availDeviceCount];
    vkEnumeratePhysicalDevices(m_pHandle, &availDeviceCount, ppAvailableDevices);

    VkPhysicalDevice pFoundDevice = nullptr;
    for (u32 i = 0; i < availDeviceCount; i++)
    {
        VkPhysicalDevice &pDevice = ppAvailableDevices[i];
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(pDevice, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && properties.apiVersion >= VK_API_VERSION_1_3)
        {
            pFoundDevice = pDevice;
            LOG_INFO(
                "GPU: {}(DRIVER: {}, VENDOR: {}, ID: {}) - Vulkan {}",
                properties.deviceName,
                properties.driverVersion,
                properties.vendorID,
                properties.deviceID,
                properties.apiVersion);

            break;
        }
    }

    if (!pFoundDevice)
    {
        LOG_ERROR("No GPU with proper Vulkan feature support.");
        return nullptr;
    }

    PhysicalDevice *pPhysicalDevice = new PhysicalDevice;
    if (!pPhysicalDevice->Init(pFoundDevice))
    {
        delete pPhysicalDevice;
        return nullptr;
    }

    return pPhysicalDevice;
}

Surface *Instance::GetWin32Surface(Win32Window *pWindow)
{
    ZoneScoped;

    VkSurfaceKHR pSurfaceHandle = nullptr;
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = (HINSTANCE)pWindow->m_pInstance,
        .hwnd = (HWND)pWindow->m_pHandle,
    };
    vkCreateWin32SurfaceKHR(m_pHandle, &surfaceInfo, nullptr, &pSurfaceHandle);

    Surface *pSurface = new Surface;
    pSurface->Init(pSurfaceHandle);

    return pSurface;
}

}  // namespace lr::Graphics