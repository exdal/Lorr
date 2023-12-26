#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/PhysicalDevice.hh"

#include "TaskGraph.hh"

namespace lr
{
struct BaseWindow;
}

namespace lr::Graphics
{
struct Renderer
{
    void init(BaseWindow *window);
    void record_tasks();
    void refresh_frame(u32 width, u32 height, u32 frame_count);
    void on_resize(u32 width, u32 height);
    void draw();

    Instance m_instance = {};
    PhysicalDevice m_physical_device = {};
    Surface m_surface = {};
    Device m_device = {};
    SwapChain m_swap_chain = {};
    TaskGraph *m_task_graph = nullptr;

    ImageID m_swap_chain_attachment = LR_NULL_ID;
    eastl::vector<ImageID> m_swap_chain_images = {};
    eastl::vector<ImageID> m_swap_chain_image_views = {};
    eastl::tuple<ImageID, ImageID> get_frame_images()
    {
        u32 frame_id = m_swap_chain.m_current_frame_id;
        return { m_swap_chain_images[frame_id], m_swap_chain_image_views[frame_id] };
    }
};
}  // namespace lr::Graphics