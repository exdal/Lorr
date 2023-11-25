#include "Renderer.hh"

#include "Window/Win32/Win32Window.hh"

#include "TaskGraph.hh"

namespace lr::Renderer
{
struct Triangle
{
    constexpr static eastl::string_view m_task_name = "Triangle";

    struct Uses
    {
        Preset::ClearColorAttachment m_color_attachment;
    } m_uses = {};

    void execute(TaskContext &tc)
    {
        auto list = tc.get_command_list();

        auto attachment = tc.as_color_attachment(m_uses.m_color_attachment, { 0.1f, 0.1f, 0.1f, 1.0f });
        auto render_size = tc.get_image_size(m_uses.m_color_attachment);
        Graphics::RenderingBeginDesc rendering_desc = {
            .m_render_area = { 0, 0, render_size.x, render_size.y },
            .m_color_attachments = attachment,
        };

        list->begin_rendering(&rendering_desc);
        // Le epic pbr rendering
        list->end_rendering();
    }
};

void FrameManager::create(const FrameManagerDesc &desc)
{
    ZoneScoped;

    m_frame_count = desc.frame_count;
    m_command_type_mask = desc.command_type_mask;
    m_renderer = desc.renderer;

    auto device = m_renderer->m_device;
    auto sc_images = device->get_swap_chain_images(m_renderer->m_swap_chain);
    for (int i = 0; i < m_frame_count; i++)
    {
        m_acquire_semas.push_back(device->create_binary_semaphore());
        m_present_semas.push_back(device->create_binary_semaphore());
        m_images.push_back(sc_images[i]);
        Graphics::ImageViewDesc viewDesc = { .m_image = sc_images[i] };
        m_views.push_back(device->create_image_view(&viewDesc));
    }
}

void FrameManager::destroy()
{
    ZoneScoped;

    auto device = m_renderer->m_device;
    for (auto view : m_views)
        device->delete_image_view(view);

    for (auto sema : m_acquire_semas)
        device->delete_semaphore(sema);

    for (auto sema : m_present_semas)
        device->delete_semaphore(sema);

    m_images.clear();
    m_views.clear();
    m_acquire_semas.clear();
    m_present_semas.clear();
}

void FrameManager::next_frame()
{
    m_current_frame = (m_current_frame + 1) % m_frame_count;
}

eastl::pair<Graphics::Image *, Graphics::ImageView *> FrameManager::get_image(u32 idx)
{
    return { m_images[idx], m_views[idx] };
}

eastl::pair<Graphics::Semaphore *, Graphics::Semaphore *> FrameManager::get_semaphores(u32 idx)
{
    return { m_acquire_semas[idx], m_present_semas[idx] };
}

void Renderer::create(BaseWindow *window)
{
    ZoneScoped;

    /// CONFIG ///
    /// TODO: Change them later
    u32 frame_count = 3;

    Graphics::InstanceDesc instance_desc = {
        .m_app_name = "Lorr",
        .m_app_version = 1,
        .m_engine_name = "Lorr",
        .m_api_version = VK_API_VERSION_1_3,
    };

    if (!m_instance.create(&instance_desc))
        return;

    m_physical_device = m_instance.get_physical_device();
    if (!m_physical_device)
        return;

    m_surface = m_instance.get_win32_surface(static_cast<Win32Window *>(window));
    if (!m_surface)
        return;

    m_physical_device->get_surface_formats(m_surface, m_surface->m_surface_formats);
    m_physical_device->get_present_modes(m_surface, m_surface->m_present_modes);
    m_physical_device->init_queue_families(m_surface);
    m_device = m_physical_device->get_logical_device();
    if (!m_device)
        return;

    refresh_frame(window->m_width, window->m_height);

    TaskGraphDesc task_graph_desc = {
        .m_frame_count = frame_count,
        .m_physical_device = m_physical_device,
        .m_device = m_device,
    };
    m_task_graph.create(&task_graph_desc);

    m_swap_chain_image = m_task_graph.use_persistent_image({});

    m_task_graph.add_task<Triangle>({
        .m_uses = { .m_color_attachment = m_swap_chain_image },
    });
    m_task_graph.present_task(m_swap_chain_image);
}

void Renderer::refresh_frame(u32 width, u32 height)
{
    ZoneScoped;

    // only way to let vulkan context know about new
    // window state is to query surface capabilities
    m_physical_device->set_surface_capabilities(m_surface);

    if (m_swap_chain)
    {
        m_device->delete_swap_chain(m_swap_chain);
        m_frame_manager.destroy();
        m_frame_manager = {};
    }

    // TODO: please properly handle this
    Graphics::SwapChainDesc swap_chain_desc = {
        .m_width = width,
        .m_height = height,
        .m_frame_count = 3,
        .m_format = Graphics::Format::R8G8B8A8_UNORM,
        .m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
    };
    m_swap_chain = m_device->create_swap_chain(m_surface, &swap_chain_desc);

    FrameManagerDesc frame_manager_desc = {
        .frame_count = 3,
        .command_type_mask = Graphics::CommandTypeMask::Graphics,
        .renderer = this,
    };
    m_frame_manager.create(frame_manager_desc);
}

void Renderer::on_resize(u32 width, u32 height)
{
    ZoneScoped;

    refresh_frame(width, height);
}

void Renderer::draw()
{
    ZoneScoped;

    auto [acquire_sema, present_sema] = m_frame_manager.get_semaphores(m_frame_manager.m_current_frame);
    auto [current_frame_image, current_frame_view] = m_frame_manager.get_image(m_frame_manager.m_current_frame);

    u32 acquired_index = 0;
    auto result = m_device->acquire_image(m_swap_chain, acquire_sema, acquired_index);
    if (result != Graphics::APIResult::Success)
        return;

    m_task_graph.set_image(m_swap_chain_image, current_frame_image, current_frame_view);
    m_task_graph.execute({
        .m_FrameIndex = m_frame_manager.m_current_frame,
        .m_pAcquireSema = acquire_sema,
        .m_pPresentSema = present_sema,
    });

    m_device->present(m_swap_chain, m_frame_manager.m_current_frame, present_sema, m_task_graph.m_graphics_queue);
    m_frame_manager.next_frame();
}

}  // namespace lr::Renderer
