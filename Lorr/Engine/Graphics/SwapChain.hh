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

struct SwapChain : Tracked<VkSwapchainKHR>
{
    void init(u32 width, u32 height, u32 frame_count, Format format, VkColorSpaceKHR color_space, VkPresentModeKHR present_mode)
    {
        m_width = width;
        m_height = height;
        m_current_frame_id = 0;
        m_frame_count = frame_count;
        m_format = format;
        m_color_space = color_space;
        m_present_mode = present_mode;
        m_acquire_semas.resize(frame_count);
        m_present_semas.resize(frame_count);
    }

    u32 get_next_frame() { return (m_current_frame_id + 1) % m_frame_count; }

    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_current_frame_id = 0;
    u32 m_frame_count = 0;
    Format m_format = Format::Unknown;

    eastl::vector<Semaphore> m_acquire_semas = {};
    eastl::vector<Semaphore> m_present_semas = {};

    VkColorSpaceKHR m_color_space = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
};
LR_ASSIGN_OBJECT_TYPE(SwapChain, VK_OBJECT_TYPE_SWAPCHAIN_KHR);

}  // namespace lr::Graphics