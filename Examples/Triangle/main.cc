#include "Engine/Core/Application.hh"

#include "Engine/Graphics/Task/BuiltinTasks.hh"

using namespace lr;
using namespace lr::graphics;

struct TriangleTask {
    std::string_view name = "Triangle";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    bool prepare(TaskPrepareInfo &info)
    {
        auto &pipeline_info = info.pipeline_info;

        VirtualFileInfo virtual_files[] = { { "lorr", embedded::lorr_sp } };
        VirtualDir virtual_dir = { virtual_files };
        ShaderCompileInfo shader_compile_info = {
            .real_path = "example_triangle.slang",
            .code = embedded::example_triangle_str,
            .virtual_env = { &virtual_dir, 1 },
        };

        shader_compile_info.entry_point = "vs_main";
        auto [vs_ir, vs_result] = ShaderCompiler::compile(shader_compile_info);
        shader_compile_info.entry_point = "fs_main";
        auto [fs_ir, fs_result] = ShaderCompiler::compile(shader_compile_info);
        if (!vs_result || !fs_result) {
            LR_LOG_ERROR("Failed to initialize Triangle pass! {}, {}", vs_result, fs_result);
            return false;
        }

        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Vertex, vs_ir));
        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Fragment, fs_ir));
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});
        pipeline_info.set_blend_attachment_all({
            .blend_enabled = true,
            .src_blend = BlendFactor::SrcAlpha,
            .dst_blend = BlendFactor::InvSrcAlpha,
            .src_blend_alpha = BlendFactor::One,
            .dst_blend_alpha = BlendFactor::InvSrcAlpha,
        });

        return true;
    }

    void execute(TaskContext &tc)
    {
        auto &task_attachment = tc.task_image_data(uses.attachment);
        auto &attachment_image = tc.device.image_at(task_attachment.image_id);
        auto render_extent = attachment_image.extent;
        auto rendering_attachment_info = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering({
            .render_area = { 0, 0, render_extent.width, render_extent.height },
            .color_attachments = rendering_attachment_info,
        });
        tc.cmd_list.set_viewport(
            0,
            { .x = 0, .y = 0, .width = static_cast<f32>(render_extent.width), .height = static_cast<f32>(render_extent.height), .depth_min = 0.01f, .depth_max = 1.0f });
        tc.cmd_list.set_scissors(0, { 0, 0, render_extent.width, render_extent.height });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct TriangleApp : Application {
    bool prepare(this TriangleApp &self)
    {
        self.task_graph.add_task<TriangleTask>({
            .uses = { .attachment = self.swap_chain_image_id },
        });
        self.task_graph.add_task<BuiltinTask::ImGuiTask>({
            .uses = {
                .attachment = self.swap_chain_image_id,
                .font = self.imgui_font_image_id,
            },
        });
        self.task_graph.present(self.swap_chain_image_id);

        return true;
    }

    bool update(this TriangleApp &self, f64 delta_time)
    {
        self.task_graph.draw_profiler_ui();
        return true;
    }

    bool do_prepare() override { return prepare(); }
    bool do_update(f64 delta_time) override { return update(delta_time); }
};

static TriangleApp app = {};

Application &Application::get()
{
    return app;
}

i32 main(i32 argc, c8 **argv)
{
    ZoneScoped;

    app.init(ApplicationInfo{
        .args = { argv, static_cast<usize>(argc) },
        .window_info = { .title = "Hello Triangle", .width = 1280, .height = 720, .flags = os::WindowFlag::Centered },
    });
    app.run();

    return 1;
}
