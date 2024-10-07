#pragma once

#include "Common.hh"
#include "CommandList.hh"

namespace lr {
struct SwapChainInfo {
    VkSurfaceKHR surface = nullptr;
    Extent2D extent = {};
};

struct SwapChain {
    SwapChain() = default;

    std::pair<Semaphore &, Semaphore &> binary_semas(usize frame_counter) { return { acquire_semas[frame_counter], present_semas[frame_counter] }; }

    Format format = Format::Unknown;
    Extent3D extent = {};

    ls::static_vector<Semaphore, Limits::FrameCount> acquire_semas;
    ls::static_vector<Semaphore, Limits::FrameCount> present_semas;

    vkb::Swapchain handle = {};
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    operator auto &() { return handle; }
    operator bool() { return handle != VK_NULL_HANDLE; }
};

}  // namespace lr
