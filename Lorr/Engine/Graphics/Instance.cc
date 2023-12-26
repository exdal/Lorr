#include "Instance.hh"

#include <EASTL/scoped_array.h>

#include "Memory/MemoryUtils.hh"
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

APIResult Instance::init(InstanceDesc *desc)
{
    m_vulkan_lib = VK::LoadVulkan();
    if (!m_vulkan_lib)
    {
        LOG_ERROR("Failed to load Vulkan library.");
        return APIResult::Unknown;
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

        return APIResult::ExtNotPresent;
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
    auto result = static_cast<APIResult>(vkCreateInstance(&createInfo, nullptr, &m_handle));
    if (result != APIResult::Success)
        return result;

    if (!VK::LoadVulkanInstance(m_handle))
    {
        LOG_ERROR("Cannot load Vulkan instance functions.");
        return APIResult::Unknown;
    }

    u32 avail_device_count = 0;
    vkEnumeratePhysicalDevices(m_handle, &avail_device_count, nullptr);
    if (avail_device_count == 0)
    {
        LOG_ERROR("No Vulkan compatible physical device detected.");
        return APIResult::DeviceLost;
    }

    m_physical_devices.resize(avail_device_count);
    vkEnumeratePhysicalDevices(m_handle, &avail_device_count, m_physical_devices.data());

    return APIResult::Success;
}

static bool check_extensions(eastl::span<VkExtensionProperties> avail_extensions, eastl::span<const char *> required_extensions)
{
    for (eastl::string_view extension : required_extensions)
    {
        bool found = false;
        for (auto &properties : avail_extensions)
        {
            if (properties.extensionName == extension)
            {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        LOG_WARN("Device extension {} is not found in this device.", extension);

        return false;
    }

    return true;
}

static eastl::vector<QueueFamilyInfo> select_queues(eastl::span<QueueSelectType> select_types, eastl::span<VkQueueFamilyProperties> propertieses)
{
    ZoneScoped;

    constexpr static VkQueueFlags type_to_flags[]{
        [(u32)CommandType::Graphics] = VK_QUEUE_GRAPHICS_BIT,
        [(u32)CommandType::Compute] = VK_QUEUE_COMPUTE_BIT,
        [(u32)CommandType::Transfer] = VK_QUEUE_TRANSFER_BIT,
    };

    auto select_queue = [=](CommandType type, QueueSelectType select_type, eastl::span<VkQueueFamilyProperties> properties) -> u32
    {
        auto flags = type_to_flags[static_cast<u32>(type)];
        switch (select_type)
        {
            case QueueSelectType::DoNotSelect:
                return ~0;
            case QueueSelectType::PreferDedicated:
            {
                u32 best_bit_count = ~0;
                u32 best_idx = ~0;
                for (u32 i = 0; i < properties.size(); i++)
                {
                    auto &v = properties[i];
                    u32 bit_count = _popcnt32(v.queueFlags);
                    if (v.queueFlags & flags && best_bit_count > bit_count)
                    {
                        best_bit_count = bit_count;
                        best_idx = i;
                    }
                }
                return best_idx;
            }
            case QueueSelectType::RequireDedicated:
            {
                for (u32 i = 0; i < properties.size(); i++)
                {
                    auto &v = properties[i];
                    if (v.queueFlags == flags)
                        return i;
                }
                return ~0;
            }
        }

        return ~0;
    };

    eastl::vector<QueueFamilyInfo> selected_families = {};

    for (u32 i = 0; i < select_types.size(); i++)
    {
        QueueSelectType select_type = select_types[i];
        if (select_type == QueueSelectType::DoNotSelect)
            continue;

        u32 queue_index = select_queue(static_cast<CommandType>(i), select_type, propertieses);
        if (queue_index == ~0)
            break;

        auto &properties = propertieses[queue_index];
        CommandTypeMask supported_types = {};
        if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            supported_types |= CommandTypeMask::Graphics;
        if (properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
            supported_types |= CommandTypeMask::Compute;
        if (properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
            supported_types |= CommandTypeMask::Transfer;

        selected_families.push_back({ .m_supported_types = supported_types, .m_queue_count = properties.queueCount, .m_index = queue_index });
    }

    return selected_families;
}

APIResult Instance::select_physical_device(PhysicalDevice *physical_device, PhysicalDeviceSelectInfo *select_info)
{
    ZoneScoped;

    u32 selected_device_id = ~0;
    eastl::vector<QueueFamilyInfo> queue_family_infos = {};
    for (u32 k = 0; k < m_physical_devices.size(); k++)
    {
        VkPhysicalDevice &device = m_physical_devices[k];

        u32 device_extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, nullptr);
        eastl::vector<VkExtensionProperties> extension_propertieses(device_extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, extension_propertieses.data());

        if (!check_extensions(extension_propertieses, select_info->m_required_extensions))
            continue;

        u32 queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
        eastl::vector<VkQueueFamilyProperties> queue_family_propertieses(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_family_propertieses.data());

        // There is a chance that `select_queues` can return same
        // type more than once, so we need to reorder it per type
        eastl::vector<QueueFamilyInfo> current_families = select_queues(select_info->m_queue_select_types, queue_family_propertieses);
        eastl::vector<QueueFamilyInfo> ordered_families = {};
        for (u32 i = 0; i < select_info->m_queue_select_types.size(); i++)
        {
            QueueSelectType select_type = select_info->m_queue_select_types[i];
            if (select_type == QueueSelectType::DoNotSelect)
                continue;

            CommandTypeMask type_mask = static_cast<CommandTypeMask>(1 << i);
            QueueFamilyInfo *best_family = nullptr;

            for (auto &queue_family : current_families)
            {
                if (queue_family.m_supported_types & type_mask
                    && (!best_family || _popcnt32((u32)best_family->m_supported_types) > _popcnt32((u32)queue_family.m_supported_types)))
                {
                    best_family = &queue_family;
                }
            }

            if (!best_family)
                return APIResult::DeviceLost;

            ordered_families.push_back(*best_family);
        }

        selected_device_id = k;
        queue_family_infos = eastl::move(ordered_families);
    }

    VkPhysicalDevice physical_device_raw = m_physical_devices[selected_device_id];
    return physical_device->init(physical_device_raw, queue_family_infos);
}

APIResult Instance::get_win32_surface(Surface *surface, Win32Window *window)
{
    ZoneScoped;

    validate_handle(surface);

    VkWin32SurfaceCreateInfoKHR surfaceInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = static_cast<HINSTANCE>(window->m_instance),
        .hwnd = static_cast<HWND>(window->m_handle),
    };
    return static_cast<APIResult>(vkCreateWin32SurfaceKHR(m_handle, &surfaceInfo, nullptr, &surface->m_handle));
}

}  // namespace lr::Graphics