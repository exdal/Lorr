#pragma once

#include "Common.hh"
#include "CommandList.hh"

namespace lr::graphics {
enum class SwapChainBuffering {
    Single = 1,
    Double = 2,
    Triple = 3,
};

struct SwapChainInfo {
    VkSurfaceKHR surface = nullptr;
    Extent2D extent = {};
    SwapChainBuffering buffering = SwapChainBuffering::Triple;
};

struct SwapChain {
    SwapChain() = default;

    usize image_count() { return m_image_count; }
    usize sema_index() { return m_frame_timeline_counter % m_image_count; }
    std::pair<Semaphore &, u64 &> frame_sema()
    {
        return { *m_frame_timeline_sema, m_frame_timeline_counter };
    }

    std::pair<Semaphore &, Semaphore &> get_binary_semas()
    {
        return { m_acquire_semas->at(sema_index()), m_present_semas->at(sema_index()) };
    }

    u64 frame_wait_val()
    {
        return static_cast<u64>(std::max<i64>(
            0, static_cast<i64>(m_frame_timeline_counter) - static_cast<i64>(m_image_count - 1)));
    }

    void inc_frame() { m_frame_timeline_counter++; }

    Format m_format = Format::Unknown;
    Extent2D m_extent = {};
    u32 m_image_count = 0;

    u64 m_frame_timeline_counter = 0;
    Unique<Semaphore> m_frame_timeline_sema;
    Unique<ls::static_vector<Semaphore, Limits::FrameCount>> m_acquire_semas;
    Unique<ls::static_vector<Semaphore, Limits::FrameCount>> m_present_semas;

    vkb::Swapchain m_handle = {};
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    constexpr static auto OBJECT_TYPE = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    operator auto &() { return m_handle; }
    operator bool() { return m_handle != VK_NULL_HANDLE; }
};

}  // namespace lr::graphics
