#include "Core/Config.hh"
#include "Graphics/Instance.hh"
#include "Graphics/Device.hh"
#include "Window/Win32/Win32Window.hh"
#include "Renderer/TaskGraph.hh"

#include <imgui.h>

using namespace lr::Graphics;

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

        m_push_constant.m_scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
        m_push_constant.m_translate = { -1.0f - draw_data->DisplayPos.x, -1.0f - draw_data->DisplayPos.y };
        m_push_constant.m_translate *= m_push_constant.m_scale;
        tc.set_push_constant(ShaderStage::Vertex, m_push_constant);

        glm::vec2 clip_offset = { draw_data->DisplayPos.x, draw_data->DisplayPos.y };
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
                tc.set_descriptors(ShaderStage::Pixel, { m_uses.m_font_image });

                list.draw_indexed(cmd.ElemCount, cmd.IdxOffset + index_offset, cmd.VtxOffset + vertex_offset);
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }

        list.set_scissors(0, { 0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y });
        list.end_rendering();
    }
};

int main(int argc, char *argv[])
{
    lr::Logger::Init();
    lr::Config::Init();

    lr::Win32Window window;
    window.init({
        .m_title = "Test",
        .m_width = 1280,
        .m_height = 720,
        .m_flags = lr::WindowFlag::Resizable | lr::WindowFlag::Centered,
    });

    Instance instance;
    instance.init({
        .m_app_name = "Test",
        .m_engine_name = "Lorr",
    });

    Device device = {};
    auto api_result = instance.create_devce(&device);

    SwapChain swap_chain = {};
    SwapChainDesc swap_chain_desc = {
        .m_window_handle = window.m_handle,
        .m_window_instance = window.m_instance,
        .m_width = window.m_width,
        .m_height = window.m_height,
        .m_frame_count = 3,
        .m_format = Format::R8G8B8A8_UNORM,
    };
    api_result = device.create_swap_chain(&swap_chain, &swap_chain_desc);

    TaskGraphDesc task_graph_desc = {
        .m_device = &device,
        .m_swap_chain = &swap_chain,
    };
    TaskGraph task_graph;
    task_graph.init(&task_graph_desc);

    auto [image_id, image_view_id] = swap_chain.get_frame_image();
    TaskImageID swap_chain_image = task_graph.use_persistent_image({ .m_image_id = image_id, .m_image_view_id = image_view_id });

    task_graph.add_task<Triangle>({
        .m_uses = { .m_color_attachment = swap_chain_image },
    });
    task_graph.submit();
    task_graph.present({ .m_swap_chain_image_id = swap_chain_image });

    while (!window.should_close())
    {
        u32 frame_id = 0;
        if (device.acquire_next_image(&swap_chain, frame_id) != APIResult::Success)
            break;

        auto [image_id, image_view_id] = swap_chain.get_frame_image(frame_id);

        task_graph.set_image(swap_chain_image, image_id, image_view_id);
        task_graph.execute({ .m_swap_chain_image_id = swap_chain_image });
        window.poll();
    }

    return 0;
}
