#include "Renderer.hh"

#include "Window/Win32/Win32Window.hh"

#include "TaskGraph.hh"

namespace lr::Graphics
{
struct Triangle
{
    constexpr static eastl::string_view m_task_name = "Triangle";

    struct Uses
    {
        Preset::ClearColorAttachment m_color_attachment;
    } m_uses = {};

    void compile_pipeline(PipelineCompileInfo &compile_info)
    {
        auto &working_dir = CONFIG_GET_VAR(SHADER_WORKING_DIR);
        compile_info.add_include_dir(working_dir);
        compile_info.add_shader("imgui.vs.hlsl");
        compile_info.add_shader("imgui.ps.hlsl");

        compile_info.set_dynamic_state(DynamicState::Viewport | DynamicState::Scissor);
        compile_info.set_viewport(0, {});
        compile_info.set_scissors(0, {});
    }

    void execute(TaskContext &tc)
    {
        auto list = tc.get_command_list();

        auto attachment = tc.as_color_attachment(m_uses.m_color_attachment);
        list.begin_rendering({ .m_color_attachments = attachment });
        list.set_viewport(0, tc.get_pass_size());
        list.set_scissors(0, tc.get_pass_size());
        list.draw(3);
        list.end_rendering();
    }
};

void Renderer::init(BaseWindow *window)
{
    ZoneScoped;

    /// CONFIG ///
    /// TODO: Change them later
    u32 frame_count = 3;

    InstanceDesc instance_desc = {
        .m_app_name = "Lorr",
        .m_app_version = 1,
        .m_engine_name = "Lorr",
        .m_api_version = VK_API_VERSION_1_3,
    };

    if (m_instance.init(&instance_desc) != APIResult::Success)
        return;

    PhysicalDeviceSelectInfo physical_device_select_info = { .m_required_extensions = PhysicalDevice::get_extensions() };
    physical_device_select_info.m_queue_select_types[0] = QueueSelectType::PreferDedicated;

    if (m_instance.select_physical_device(&m_physical_device, &physical_device_select_info) != APIResult::Success)
        return;

    if (m_instance.get_win32_surface(&m_surface, static_cast<Win32Window *>(window)) != APIResult::Success)
        return;

    m_surface.init(&m_physical_device);

    m_physical_device.get_logical_device(&m_device);
    if (!m_device)
        return;

    refresh_frame(window->m_width, window->m_height);

    TaskGraphDesc task_graph_desc = {
        .m_device = &m_device,
        .m_swap_chain = &m_swap_chain,
    };
    m_task_graph.init(&task_graph_desc);

    m_swap_chain_image = m_task_graph.use_persistent_image({});

    m_task_graph.add_task<Triangle>({
        .m_uses = { .m_color_attachment = m_swap_chain_image },
    });
}

void Renderer::refresh_frame(u32 width, u32 height)
{
    ZoneScoped;

    // only way to let vulkan context know about new
    // window state is to query surface capabilities
    m_surface.refresh_surface_capabilities(&m_physical_device);

    if (m_swap_chain)
    {
        m_device.delete_swap_chain(&m_swap_chain);
    }

    // TODO: please properly handle this
    SwapChainDesc swap_chain_desc = {
        .m_width = width,
        .m_height = height,
        .m_frame_count = 3,
        .m_format = Format::R8G8B8A8_UNORM,
        .m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
    };
    m_device.create_swap_chain(&m_swap_chain, &swap_chain_desc, &m_surface);
}

void Renderer::on_resize(u32 width, u32 height)
{
    ZoneScoped;

    refresh_frame(width, height);
}

void Renderer::draw()
{
    ZoneScoped;

    u32 image_id = 0;
    if (m_device.acquire_next_image(&m_swap_chain, image_id) != APIResult::Success)
        return;

    m_task_graph.set_image(m_swap_chain_image, &m_swap_chain.m_images[image_id], &m_swap_chain.m_image_views[image_id]);
    m_task_graph.execute(&m_swap_chain);
}

}  // namespace lr::Graphics
