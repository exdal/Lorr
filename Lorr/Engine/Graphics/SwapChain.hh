#pragma once

#include "Common.hh"
#include "CommandList.hh"

namespace lr::graphics {
struct SwapChainInfo {
    VkSurfaceKHR surface = nullptr;
    Extent2D extent = {};
};

struct SwapChain {
    SwapChain() = default;

    std::pair<Semaphore &, Semaphore &> binary_semas(usize frame_counter) { return { m_acquire_semas[frame_counter], m_present_semas[frame_counter] }; }

    Format m_format = Format::Unknown;
    Extent2D m_extent = {};

    ls::static_vector<Semaphore, Limits::FrameCount> m_acquire_semas;
    ls::static_vector<Semaphore, Limits::FrameCount> m_present_semas;

    vkb::Swapchain m_handle = {};
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    operator auto &() { return m_handle; }
    operator bool() { return m_handle != VK_NULL_HANDLE; }
};

}  // namespace lr::graphics
