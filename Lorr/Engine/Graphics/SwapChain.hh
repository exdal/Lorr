#pragma once

#include "APIObject.hh"
#include "Common.hh"

namespace lr::Graphics
{
struct SwapChainDesc
{
    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_FrameCount = 0;
    Format m_Format = Format::Unknown;
    VkColorSpaceKHR m_ColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
};

struct SwapChain : APIObject
{
    bool VSynced() { return m_FrameCount == 1; }

    VkSwapchainKHR m_pHandle = nullptr;

    u32 m_CurrentFrame = 0;
    u32 m_FrameCount = 2;
    u32 m_Width = 0;
    u32 m_Height = 0;

    Format m_ImageFormat = Format::BGRA8_UNORM;
    VkColorSpaceKHR m_ColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
};
LR_ASSIGN_OBJECT_TYPE(SwapChain, VK_OBJECT_TYPE_SWAPCHAIN_KHR);

}  // namespace lr::Graphics