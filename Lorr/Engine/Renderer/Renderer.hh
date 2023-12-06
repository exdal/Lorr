#pragma once

#include "Graphics/Device.hh"
#include "Graphics/Instance.hh"
#include "Graphics/PhysicalDevice.hh"

#include "TaskGraph.hh"

namespace lr
{
struct BaseWindow;
}

namespace lr::Renderer
{
struct Renderer;
struct FrameManagerDesc
{
    u32 frame_count = 0;
    Graphics::CommandTypeMask command_type_mask = {};
    Renderer *renderer = nullptr;
};

struct FrameManager
{
    void init(const FrameManagerDesc &desc);
    void destroy();
    void next_frame();  // Advance to the next frame
    eastl::pair<Graphics::Image *, Graphics::ImageView *> get_image(u32 idx = 0);
    eastl::pair<Graphics::Semaphore *, Graphics::Semaphore *> get_semaphores(u32 idx = 0);

    u32 m_frame_count = 0;
    u32 m_current_frame = 0;
    Graphics::CommandTypeMask m_command_type_mask = {};
    Renderer *m_renderer = nullptr;

    // Prsentation Engine
    eastl::vector<Graphics::Semaphore *> m_acquire_semas = {};
    eastl::vector<Graphics::Semaphore *> m_present_semas = {};
    eastl::vector<Graphics::Image *> m_images = {};
    eastl::vector<Graphics::ImageView *> m_views = {};
};

struct Renderer
{
    void init(BaseWindow *window);
    void refresh_frame(u32 width, u32 height);
    void on_resize(u32 width, u32 height);
    void draw();

    void load_pipelines();

    auto &get_pipeline_manager() { return m_task_graph.m_pipeline_manager; }

    Graphics::Instance m_instance = {};
    Graphics::PhysicalDevice *m_physical_device = nullptr;
    Graphics::Surface *m_surface = nullptr;
    Graphics::Device *m_device = nullptr;
    Graphics::SwapChain *m_swap_chain = nullptr;

    FrameManager m_frame_manager = {};
    TaskGraph m_task_graph = {};

    ImageID m_swap_chain_image = ImageNull;
};
}  // namespace lr::Renderer