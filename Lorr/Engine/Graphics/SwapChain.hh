#pragma once

#include "APIObject.hh"
#include "CommandList.hh"
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

struct SwapChain
{
    u32 m_current_frame_id = 0;
    u32 m_frame_count = 0;
    eastl::vector<Semaphore> m_acquire_semas = {};
    eastl::vector<Semaphore> m_present_semas = {};
    eastl::vector<Image> m_images = {};
    eastl::vector<ImageView> m_image_views = {};

    VkColorSpaceKHR m_color_space = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;

    VkSwapchainKHR m_handle = nullptr;

    operator VkSwapchainKHR &() { return m_handle; }
    explicit operator bool() { return m_handle != nullptr; }
};
LR_ASSIGN_OBJECT_TYPE(SwapChain, VK_OBJECT_TYPE_SWAPCHAIN_KHR);

}  // namespace lr::Graphics