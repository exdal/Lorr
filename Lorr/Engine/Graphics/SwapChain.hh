#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
struct SwapChainDesc
{
    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_frame_count = 0;
    Format m_format = Format::Unknown;
    VkColorSpaceKHR m_color_space = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
};

struct SwapChain : APIObject
{
    VkSwapchainKHR m_handle = nullptr;

    u32 m_frame_count = 2;
    u32 m_width = 0;
    u32 m_height = 0;

    Format m_image_format = Format::BGRA8_UNORM;
    VkColorSpaceKHR m_color_space = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;

    operator VkSwapchainKHR &() { return m_handle; }
};
LR_ASSIGN_OBJECT_TYPE(SwapChain, VK_OBJECT_TYPE_SWAPCHAIN_KHR);

}  // namespace lr::Graphics