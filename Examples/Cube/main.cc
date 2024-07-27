#include "Engine/Core/Application.hh"

#include "Engine/Graphics/Task/BuiltinTasks.hh"

using namespace lr;

struct CubeTask {
    std::string_view name = "Cube";

    struct Uses {
        Preset::ColorAttachmentWrite color_attachment = {};
        Preset::DepthAttachmentWrite depth_attachment = {};
    } uses = {};

    struct PushConstants {
        glm::mat4 view_proj_mat = {};
    } push_constants = {};

    bool prepare(TaskPrepareInfo &info)
    {
        auto &pipeline_info = info.pipeline_info;

        VirtualFileInfo virtual_files[] = { { "lorr", embedded::lorr_sp } };
        VirtualDir virtual_dir = { virtual_files };
        ShaderCompileInfo shader_compile_info = {
            .real_path = "example_cube.slang",
            .code = embedded::example_cube_str,
            .virtual_env = { &virtual_dir, 1 },
        };

        shader_compile_info.entry_point = "vs_main";
        auto [vs_ir, vs_result] = ShaderCompiler::compile(shader_compile_info);
        shader_compile_info.entry_point = "fs_main";
        auto [fs_ir, fs_result] = ShaderCompiler::compile(shader_compile_info);
        if (!vs_result || !fs_result) {
            LR_LOG_ERROR("Failed to initialize Cube pass! {}, {}", vs_result, fs_result);
            return false;
        }

        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Vertex, vs_ir));
        pipeline_info.set_shader(info.device->create_shader(ShaderStageFlag::Fragment, fs_ir));
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});
        pipeline_info.set_rasterizer_state({ .cull_mode = CullMode::Back });
        pipeline_info.set_depth_stencil_state({ .enable_depth_test = true, .enable_depth_write = true, .depth_compare_op = CompareOp::LessEqual });
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
        auto &app = Application::get();
        auto color_attachment_info = tc.as_color_attachment(uses.color_attachment, ColorClearValue(0.01f, 0.01f, 0.01f, 1.0f));
        auto depth_attachment_info = tc.as_depth_attachment(uses.depth_attachment, DepthClearValue(1.0f));

        push_constants.view_proj_mat = app.camera.proj_view_matrix;
        tc.set_push_constants(push_constants);

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment_info,
            .depth_attachment = depth_attachment_info,
        });
        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());
        tc.cmd_list.draw(36);
        tc.cmd_list.end_rendering();
    }
};

struct CubeApp : Application {
    ImageID depth_attachment_id = {};
    ImageViewID depth_attachment_view_id = {};

    bool prepare(this CubeApp &self)
    {
        self.depth_attachment_id = self.device.create_image(ImageInfo{
            .usage_flags = ImageUsage::DepthStencilAttachment,
            .format = Format::D32_SFLOAT,
            .extent = { 1280, 720, 1 },
            .debug_name = "Depth Attachment Image",
        });

        self.depth_attachment_view_id = self.device.create_image_view(ImageViewInfo{
            .image_id = self.depth_attachment_id,
            .usage_flags = ImageUsage::DepthStencilAttachment,
            .type = ImageViewType::View2D,
            .subresource_range = { .aspect_mask = ImageAspect::Depth },
            .debug_name = "Depth Attachment ImageView",
        });
        auto depth_attachment_id = self.task_graph.add_image(TaskPersistentImageInfo{
            .image_id = self.depth_attachment_id,
            .image_view_id = self.depth_attachment_view_id,
        });

        self.task_graph.add_task<CubeTask>({
            .uses = {
                .color_attachment = self.swap_chain_image_id,
                .depth_attachment = depth_attachment_id, 
            },
        });
        self.task_graph.add_task<BuiltinTask::ImGuiTask>({
            .uses = {
                .attachment = self.swap_chain_image_id,
                .font = self.imgui_font_image_id,
            },
        });
        self.task_graph.present(self.swap_chain_image_id);

        self.camera = PerspectiveCamera({ 0.0f, 0.0f, -5.0f }, 60.0f, 1.6f);
        return true;
    }

    bool update(this CubeApp &self, f32 delta_time)
    {
        ImGui::SetNextWindowContentSize({ 400, 200 });
        ImGui::Begin("Camera");
        ImGui::SliderFloat3("Position", &self.camera.position.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Yaw", &self.camera.yaw, -90.0f, 90.0f);
        ImGui::SliderFloat("Pitch", &self.camera.pitch, -89.0f, 89.0f);
        ImGui::End();

        return true;
    }

    bool do_prepare() override { return prepare(); }
    bool do_update(f32 delta_time) override { return update(delta_time); }
};

static CubeApp app = {};

Application &Application::get()
{
    return app;
}

i32 main(i32 argc, c8 **argv)
{
    ZoneScoped;

    app.init(ApplicationInfo{
        .args = { argv, static_cast<usize>(argc) },
        .window_info = { .title = "Hello Cube", .width = 1280, .height = 720, .flags = WindowFlag::Centered },
    });

    return 1;
}
