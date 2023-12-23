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
    void refresh_frame(u32 width, u32 height);
    void on_resize(u32 width, u32 height);
    void draw();

    auto &get_pipeline_manager() { return m_task_graph.m_pipeline_manager; }

    Instance m_instance = {};
    PhysicalDevice m_physical_device = {};
    Surface m_surface = {};
    Device m_device = {};
    SwapChain m_swap_chain = {};
    TaskGraph m_task_graph = {};

    ImageID m_swap_chain_image = LR_NULL_ID;
};
}  // namespace lr::Renderer