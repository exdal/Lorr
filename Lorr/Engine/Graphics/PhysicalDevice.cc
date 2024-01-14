#include "PhysicalDevice.hh"

#include "Device.hh"

namespace lr::Graphics
{
APIResult Surface::init(PhysicalDevice *physical_device)
{


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



}  // namespace lr::Graphics