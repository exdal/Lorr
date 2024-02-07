#include "Core/Config.hh"
#include "Graphics/Instance.hh"
#include "Graphics/Device.hh"
#include "Window/Win32/Win32Window.hh"
#include "Renderer/TaskGraph.hh"
#include "Utils/Timer.hh"

#include <imgui.h>

using namespace lr;
using namespace lr::Graphics;

struct Triangle {
    constexpr static eastl::string_view m_task_name = "Triangle";

    struct Uses {
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
        compile_info.set_input_layout({});
    }

    void execute(TaskContext &tc)
    {
        auto &list = tc.get_command_list();
        auto attachment = tc.as_color_attachment(m_uses.m_color_attachment);
        glm::uvec4 pass_size = tc.get_pass_size();

        list.begin_rendering({ .m_render_area = pass_size, .m_color_attachments = attachment });
        list.set_pipeline(tc.get_pipeline());
        list.set_viewport(0, { pass_size.x, pass_size.y }, { pass_size.z, pass_size.w });
        list.set_scissors(0, { pass_size.x, pass_size.y }, { pass_size.z, pass_size.w });
        list.draw(3);
        list.end_rendering();
    }
};

struct Imgui {
    constexpr static eastl::string_view m_task_name = "Imgui";

    struct Uses {
        Preset::ClearColorAttachment m_color_attachment;
        // Preset::PixelReadOnly m_font_image;
    } m_uses = {};

    struct PushConstant {
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
        compile_info.set_input_layout({ Format::R32G32_SFLOAT, Format::R32G32_SFLOAT, Format::R8G8B8A8_UNORM });
        compile_info.set_push_constants<PushConstant>();
    }

    void execute(TaskContext &tc)
    {
        auto &list = tc.get_command_list();
        auto attachment = tc.as_color_attachment(m_uses.m_color_attachment);

        ImDrawData *draw_data = ImGui::GetDrawData();
        if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f || draw_data->TotalVtxCount == 0)
            return;

        usize vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        auto [vertex_buf_id, vertex_data] = tc.allocate_transient_buffer({
            .m_usage_flags = BufferUsage::Vertex,
            .m_data_size = vertex_size,
        });
        usize index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        auto [index_buf_id, index_data] = tc.allocate_transient_buffer({
            .m_usage_flags = BufferUsage::Index,
            .m_data_size = index_size,
        });

        auto vtx_dst = (ImDrawVert *)vertex_data;
        auto idx_dst = (ImDrawIdx *)index_data;
        for (i32 n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        list.begin_rendering({ .m_render_area = tc.get_pass_size(), .m_color_attachments = attachment });
        list.set_pipeline(tc.get_pipeline());
        list.set_viewport(0, { 0, 0 }, { draw_data->DisplaySize.x, draw_data->DisplaySize.y });
        list.set_vertex_buffer(tc.get_buffer(vertex_buf_id));
        list.set_index_buffer(tc.get_buffer(index_buf_id), 0, sizeof(ImDrawIdx) == sizeof(u16));

        glm::vec2 display_pos = { draw_data->DisplayPos.x, draw_data->DisplayPos.y };
        glm::vec2 display_size = { draw_data->DisplaySize.x, draw_data->DisplaySize.y };
        glm::vec2 view_size = 2.0f / glm::vec2(draw_data->DisplaySize.x, draw_data->DisplaySize.y);
        m_push_constant = { view_size, -1.0f - (display_pos * view_size) };
        tc.set_push_constants(ShaderStage::Vertex, m_push_constant);

        glm::vec2 clip_offset = { draw_data->DisplayPos.x, draw_data->DisplayPos.y };
        u32 vertex_offset = 0;
        u32 index_offset = 0;
        for (auto &draw_list : draw_data->CmdLists) {
            for (auto &cmd : draw_list->CmdBuffer) {
                glm::ivec2 clip_min(cmd.ClipRect.x - clip_offset.x, cmd.ClipRect.y - clip_offset.y);
                glm::uvec2 clip_max(cmd.ClipRect.z - clip_offset.x, cmd.ClipRect.w - clip_offset.y);
                if (clip_min.x < 0) {
                    clip_min.x = 0;
                }
                if (clip_min.y < 0) {
                    clip_min.y = 0;
                }
                if (clip_max.x > display_size.x) {
                    clip_max.x = display_size.x;
                }
                if (clip_max.y > display_size.y) {
                    clip_max.y = display_size.y;
                }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                list.set_scissors(0, clip_min, clip_max - glm::uvec2(clip_min));
                // tc.set_descriptors(ShaderStage::Pixel, { m_uses.m_font_image });

                list.draw_indexed(cmd.ElemCount, cmd.IdxOffset + index_offset, cmd.VtxOffset + vertex_offset);
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }

        list.set_scissors(0, { 0, 0 }, display_size);
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
        .m_flags = lr::WindowFlag::Centered,
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

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    io.BackendPlatformName = "imgui_impl_lorr";

    u8 *imgui_font_data = nullptr;
    i32 font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&imgui_font_data, &font_width, &font_height);
    Memory::Release(imgui_font_data);

    // task_graph.add_task<Triangle>({
    //    .m_uses = { .m_color_attachment = swap_chain_image },
    // });
    task_graph.add_task<Imgui>({
        .m_uses = { .m_color_attachment = swap_chain_image },
    });
    task_graph.submit();
    task_graph.present({ .m_swap_chain_image_id = swap_chain_image });

    lr::Timer<f32> timer = {};
    while (!window.should_close()) {
        u32 frame_id = 0;
        if (device.acquire_next_image(&swap_chain, frame_id) != APIResult::Success)
            break;

        auto [image_id, image_view_id] = swap_chain.get_frame_image(frame_id);
        auto &io = ImGui::GetIO();

        auto &event_man = window.m_event_manager;
        while (event_man.peek()) {
            WindowEventData event_data = {};
            Event event = event_man.dispatch(event_data);
            switch (event) {
                case LR_WINDOW_EVENT_QUIT:
                    window.m_should_close = true;
                    break;
                case LR_WINDOW_EVENT_MOUSE_POSITION:
                    io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
                    io.AddMousePosEvent((f32)event_data.m_mouse_x, (f32)event_data.m_mouse_y);
                    break;
                case LR_WINDOW_EVENT_MOUSE_STATE:
                    io.AddMouseButtonEvent(event_data.m_mouse - 1, event_data.m_mouse_state == LR_MOUSE_STATE_DOWN);
                    break;
                case LR_WINDOW_EVENT_CURSOR_STATE:
                    window.set_cursor(event_data.m_window_cursor);
                    break;
                default:
                    break;
            }
        }

        window.set_cursor((WindowCursor)ImGui::GetMouseCursor());

        io.DeltaTime = timer.elapsed();
        io.DisplaySize = { (f32)window.m_width, (f32)window.m_height };
        ImGui::NewFrame();

        ImGui::Begin("Hello");
        ImGui::Button("Test");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::Text("This is an ImGui text being rendered.");
        ImGui::End();

        ImGui::EndFrame();
        ImGui::Render();

        task_graph.set_image(swap_chain_image, image_id, image_view_id);
        task_graph.execute({ .m_swap_chain_image_id = swap_chain_image });

        window.poll();
        timer.reset();
    }

    device.wait_for_work();

    return 0;
}
