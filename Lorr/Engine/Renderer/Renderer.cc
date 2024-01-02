#include "Renderer.hh"

#include "Window/Win32/Win32Window.hh"

#include "TaskGraph.hh"

#include <imgui.h>

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
        compile_info.add_shader(working_dir + "/triangle.vs.hlsl");
        compile_info.add_shader(working_dir + "/triangle.ps.hlsl");

        compile_info.set_dynamic_state(DynamicState::Viewport | DynamicState::Scissor);
        compile_info.set_viewport(0, {});
        compile_info.set_scissors(0, {});
        compile_info.set_blend_state_all({ true });
    }

    void execute(TaskContext &tc)
    {
        auto &list = tc.get_command_list();
        auto attachment = tc.as_color_attachment(m_uses.m_color_attachment);

        list.begin_rendering({ .m_render_area = tc.get_pass_size(), .m_color_attachments = attachment });
        list.set_pipeline(tc.get_pipeline());
        list.set_viewport(0, tc.get_pass_size());
        list.set_scissors(0, tc.get_pass_size());
        list.draw(3);
        list.end_rendering();
    }
};

struct Imgui
{
    constexpr static eastl::string_view m_task_name = "Imgui";

    struct Uses
    {
        Preset::ColorAttachment m_color_attachment;
        Preset::PixelReadOnly m_font_image;
    } m_uses = {};

    struct PushConstant
    {
        glm::vec2 m_translate = {};
        glm::vec2 m_scale = {};
    } m_push_constant = {};

    void compile_pipeline(PipelineCompileInfo &compile_info)
    {
        auto &working_dir = CONFIG_GET_VAR(SHADER_WORKING_DIR);
        compile_info.add_include_dir(working_dir);
        compile_info.add_shader(working_dir + "/imgui.vs.hlsl");
        compile_info.add_shader(working_dir + "/imgui.ps.hlsl");

        compile_info.set_dynamic_state(DynamicState::Viewport | DynamicState::Scissor);
        compile_info.set_viewport(0, {});
        compile_info.set_scissors(0, {});
        compile_info.set_blend_state_all({ true });
    }

    void execute(TaskContext &tc)
    {
        auto &list = tc.get_command_list();
        auto attachment = tc.as_color_attachment(m_uses.m_color_attachment);

        ImDrawData *draw_data = ImGui::GetDrawData();
        if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f || draw_data->TotalVtxCount == 0)
            return;

        list.begin_rendering({ .m_render_area = tc.get_pass_size(), .m_color_attachments = attachment });
        list.set_pipeline(tc.get_pipeline());

        list.set_viewport(0, { 0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y });
        glm::vec2 clip_offset = { draw_data->DisplayPos.x, draw_data->DisplayPos.y };

        m_push_constant.m_scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
        m_push_constant.m_translate = { -1.0f - draw_data->DisplayPos.x, -1.0f - draw_data->DisplayPos.y };
        m_push_constant.m_translate *= m_push_constant.m_scale;
        list.set_push_constants(&m_push_constant, sizeof(PushConstant), 4);

        u32 vertex_offset = 0;
        u32 index_offset = 0;
        for (auto &draw_list : draw_data->CmdLists)
        {
            for (auto &cmd : draw_list->CmdBuffer)
            {
                ImVec2 clip_min(cmd.ClipRect.x - clip_offset.x, cmd.ClipRect.y - clip_offset.y);
                ImVec2 clip_max(cmd.ClipRect.z - clip_offset.x, cmd.ClipRect.w - clip_offset.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                list.set_scissors(0, { clip_min.x, clip_min.y, clip_max.x, clip_max.y });

                list.draw_indexed(cmd.ElemCount, cmd.IdxOffset + index_offset, cmd.VtxOffset + vertex_offset);
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }

        list.set_scissors(0, { 0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y });
        list.end_rendering();
    }
};

void Renderer::init(BaseWindow *window)
{
    ZoneScoped;

    InstanceDesc instance_desc = {
        .m_window = static_cast<Win32Window *>(window),
        .m_app_name = "Lorr",
        .m_app_version = 1,
        .m_engine_name = "Lorr",
        .m_api_version = VK_API_VERSION_1_3,
    };

    if (m_instance.init(&instance_desc) != APIResult::Success)
        return;

    if (m_instance.get_win32_surface(&m_surface) != APIResult::Success)
        return;

    if (m_instance.get_physical_device(&m_physical_device) != APIResult::Success)
        return;

    if (m_surface.init(&m_physical_device) != APIResult::Success)
        return;

    if (m_instance.get_logical_device(&m_device, &m_physical_device) != APIResult::Success)
        return;

    refresh_frame(window->m_width, window->m_height, 3);
}

void Renderer::record_tasks()
{
    ZoneScoped;

    m_task_graph->add_task<Triangle>({
        .m_uses = { .m_color_attachment = m_swap_chain_attachment },
    });
    m_task_graph->add_task<Imgui>({
        .m_uses = { .m_color_attachment = m_swap_chain_attachment },
    });
    m_task_graph->present({ .m_swap_chain_image_id = m_swap_chain_attachment });
}

void Renderer::refresh_frame(u32 width, u32 height, u32 frame_count)
{
    ZoneScoped;

    // only way to let vulkan context know about new
    // window state is to query surface capabilities
    m_surface.refresh_surface_capabilities(&m_physical_device);

    m_device.wait_for_work();
    delete m_task_graph;
    if (m_swap_chain)
        m_device.delete_swap_chain(&m_swap_chain);

    // TODO: please properly handle this
    SwapChainDesc swap_chain_desc = {
        .m_width = width,
        .m_height = height,
        .m_frame_count = frame_count,
        .m_format = Format::R8G8B8A8_UNORM,
        .m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR,
    };
    m_device.create_swap_chain(&m_swap_chain, &swap_chain_desc, &m_surface);

    TaskGraphDesc task_graph_desc = {
        .m_device = &m_device,
        .m_frame_count = m_swap_chain.m_frame_count,
    };
    m_task_graph = new TaskGraph;
    m_task_graph->init(&task_graph_desc);

    eastl::vector<Image *> image_handles = {};
    eastl::vector<ImageView *> image_view_handles = {};

    for (u32 i = 0; i < m_swap_chain.m_frame_count; i++)
    {
        auto [image_id, image] = m_task_graph->add_new_image();
        image_handles.push_back(image);
        m_swap_chain_images.push_back(image_id);

        auto [image_view_id, image_view] = m_task_graph->add_new_image_view();
        image_view_handles.push_back(image_view);
        m_swap_chain_image_views.push_back(image_view_id);
    }
    m_device.get_swap_chain_images(&m_swap_chain, image_handles);
    for (u32 i = 0; i < m_swap_chain.m_frame_count; i++)
    {
        ImageView *image_view = image_view_handles[i];
        ImageViewDesc image_view_desc = { .m_image = image_handles[i] };
        m_device.create_image_view(image_view, &image_view_desc);
    }

    auto [image_id, image_view_id] = get_frame_images();
    m_swap_chain_attachment = m_task_graph->use_persistent_image({ .m_image_id = image_id, .m_image_view_id = image_view_id });

    record_tasks();
}

void Renderer::on_resize(u32 width, u32 height)
{
    ZoneScoped;

    refresh_frame(width, height, m_swap_chain.m_frame_count);
}

void Renderer::draw()
{
    ZoneScoped;

    u32 frame_id = 0;
    if (m_device.acquire_next_image(&m_swap_chain, frame_id) != APIResult::Success)
        return;

    auto [image_id, image_view_id] = get_frame_images();
    m_task_graph->set_image(m_swap_chain_attachment, image_id, image_view_id);

    auto wait_sema = &m_swap_chain.m_acquire_semas[frame_id];
    auto signal_sema = &m_swap_chain.m_present_semas[frame_id];
    m_task_graph->execute({ .m_swap_chain = &m_swap_chain, .m_wait_semas = wait_sema, .m_signal_semas = signal_sema });
}

}  // namespace lr::Graphics
