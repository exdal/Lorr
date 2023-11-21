#include "Instance.hh"

#include <EASTL/scoped_array.h>

#include "Window/Win32/Win32Window.hh"

#include "PhysicalDevice.hh"
#include "Vulkan.hh"

namespace lr::Graphics
{
static const char *kpp_required_extensions[] = {
    "VK_KHR_surface",
    "VK_KHR_get_physical_device_properties2",
    "VK_KHR_win32_surface",
#if _DEBUG
    //! Debug extensions, always put it to bottom
    "VK_EXT_debug_utils",
    "VK_EXT_debug_report",
#endif
};

static constexpr eastl::span<const char *> k_required_extensions(kpp_required_extensions);

bool Instance::create(InstanceDesc *desc)
{
    constexpr u32 kTypeMem = Memory::MiBToBytes(16);
    APIAllocator::m_g_handle.m_type_allocator.Init(kTypeMem, 0x2000);
    APIAllocator::m_g_handle.m_type_data = Memory::Allocate<u8>(kTypeMem);

    m_vulkan_lib = VK::LoadVulkan();
    if (!m_vulkan_lib)
    {
        LOG_ERROR("Failed to load Vulkan.");
        return false;
    }

    u32 avail_extensions = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &avail_extensions, nullptr);
    eastl::scoped_array extensions(new VkExtensionProperties[avail_extensions]);
    vkEnumerateInstanceExtensionProperties(nullptr, &avail_extensions, extensions.get());

    for (eastl::string_view extension : k_required_extensions)
    {
        bool found = false;
        for (u32 i = 0; i < avail_extensions; i++)
        {
            VkExtensionProperties &properties = extensions[i];
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
        .pApplicationName = desc->m_app_name.data(),
        .applicationVersion = desc->m_app_version,
        .pEngineName = desc->m_engine_name.data(),
        .engineVersion = desc->m_engine_version,
        .apiVersion = desc->m_api_version,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<u32>(k_required_extensions.size()),
        .ppEnabledExtensionNames = k_required_extensions.data(),
    };
    vkCreateInstance(&createInfo, nullptr, &m_handle);

    if (!VK::LoadVulkanInstance(m_handle))
        return false;

    return true;
}

PhysicalDevice *Instance::get_physical_device()
{
    ZoneScoped;

    u32 avail_device_count = 0;
    vkEnumeratePhysicalDevices(m_handle, &avail_device_count, nullptr);

    if (avail_device_count == 0)
    {
        LOG_ERROR("No GPU with Vulkan support.");
        return nullptr;
    }

    VkPhysicalDevice *available_devices = new VkPhysicalDevice[avail_device_count];
    vkEnumeratePhysicalDevices(m_handle, &avail_device_count, available_devices);

    VkPhysicalDevice found_device = nullptr;
    for (u32 i = 0; i < avail_device_count; i++)
    {
        VkPhysicalDevice &device = available_devices[i];
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && properties.apiVersion >= VK_API_VERSION_1_3)
        {
            found_device = device;
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

    if (!found_device && !check_physical_device_extensions(found_device, k_required_extensions))
    {
        LOG_ERROR("No GPU with proper Vulkan feature support.");
        return nullptr;
    }

    return new PhysicalDevice(found_device);
}
bool Instance::check_physical_device_extensions(VkPhysicalDevice physical_device, eastl::span<const char *> required_extensions)
{
    u32 avail_extensions = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &avail_extensions, nullptr);
    eastl::scoped_array extension_properties(new VkExtensionProperties[avail_extensions]);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &avail_extensions, extension_properties.get());

    for (eastl::string_view extension : required_extensions)
    {
        bool found = false;
        for (u32 i = 0; i < avail_extensions; i++)
        {
            VkExtensionProperties &properties = extension_properties[i];
            if (properties.extensionName == extension)
            {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        return false;
    }

    return true;
}

Surface *Instance::get_win32_surface(Win32Window *window)
{
    ZoneScoped;

    VkSurfaceKHR surface_handle = nullptr;
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = static_cast<HINSTANCE>(window->m_instance),
        .hwnd = static_cast<HWND>(window->m_handle),
    };
    vkCreateWin32SurfaceKHR(m_handle, &surfaceInfo, nullptr, &surface_handle);

    return new Surface(surface_handle);
}

}  // namespace lr::Graphics