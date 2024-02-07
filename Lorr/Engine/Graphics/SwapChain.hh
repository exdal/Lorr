#pragma once

#include "APIObject.hh"
#include "CommandList.hh"
#include "Common.hh"

namespace lr::Graphics {
struct SwapChainDesc {
    void *m_window_handle = nullptr;
    void *m_window_instance = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_frame_count = 0;
    Format m_format = Format::Unknown;
    VkColorSpaceKHR m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    PresentMode m_present_mode = PresentMode::Mailbox;
};

struct SwapChain {
    void init(
        VkSwapchainKHR swapchain_handle,
        u32 width,
        u32 height,
        u32 frame_count,
        Format format,
        eastl::vector<ImageID> &&images,
        eastl::vector<ImageViewID> &&image_views,
        eastl::vector<Semaphore> &&acquire_semas,
        eastl::vector<Semaphore> &&present_semas,
        VkSurfaceKHR surface)
    {
        m_handle = swapchain_handle;
        m_width = width;
        m_height = height;
        m_format = format;
        m_images = images;
        m_image_views = image_views;
        m_acquire_semas = acquire_semas;
        m_present_semas = present_semas;
        m_surface = surface;
    }

    u32 next_semaphore() { return (m_current_semaphore_id + 1) % m_present_semas.size(); }
    u32 frame_count() { return m_images.size(); }
    u32 semaphore_count() { return m_present_semas.size(); }
    eastl::tuple<ImageID, ImageViewID> get_frame_image(u32 frame_id = ~0)
    {
        if (frame_id == ~0)
            frame_id = m_current_frame_id;
        return { m_images[frame_id], m_image_views[frame_id] };
    }
    eastl::tuple<Semaphore *, Semaphore *> get_semaphores()
    {
        return { &m_acquire_semas[m_current_semaphore_id], &m_present_semas[m_current_semaphore_id] };
    }

    u32 m_width = 0;
    u32 m_height = 0;
    u32 m_current_frame_id = 0;
    u32 m_current_semaphore_id = 0;
    Format m_format = Format::Unknown;

    eastl::vector<ImageID> m_images = {};
    eastl::vector<ImageViewID> m_image_views = {};
    eastl::vector<Semaphore> m_acquire_semas = {};
    eastl::vector<Semaphore> m_present_semas = {};

    VkSurfaceKHR m_surface = nullptr;
    VkSwapchainKHR m_handle = VK_NULL_HANDLE;
};
LR_ASSIGN_OBJECT_TYPE(SwapChain, VK_OBJECT_TYPE_SWAPCHAIN_KHR);

}  // namespace lr::Graphics
