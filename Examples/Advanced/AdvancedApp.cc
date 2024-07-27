#include "AdvancedApp.hh"

#include <Embedded.hh>

#include "Engine/Graphics/Task/BuiltinTasks.hh"

#include "Engine/Asset/Asset.hh"

struct GeometryTask {
    std::string_view name = "Geometry Task";

    struct Uses {
        lr::Preset::ColorAttachmentWrite color_attachment = {};
        lr::Preset::DepthAttachmentWrite depth_attachment = {};
    } uses = {};

    struct PushConstants {
        glm::mat4 camera_mat = {};
        lr::BufferID material_buffer_id = {};
        u32 material_id = {};
    } push_constants = {};

    struct {
        lr::ModelID model_id = lr::ModelID::Invalid;
    } self = {};

    bool prepare(lr::TaskPrepareInfo &info) {
        auto &pipeline_info = info.pipeline_info;

        lr::VirtualFileInfo virtual_files[] = { { "lorr", lr::embedded::lorr_sp } };
        lr::VirtualDir virtual_dir = { virtual_files };
        lr::ShaderCompileInfo shader_compile_info = {
            .real_path = "example_geometry.slang",
            .code = lr::embedded::example_geometry_str,
            .virtual_env = { &virtual_dir, 1 },
        };

        shader_compile_info.entry_point = "vs_main";
        auto [vs_ir, vs_result] = lr::ShaderCompiler::compile(shader_compile_info);
        shader_compile_info.entry_point = "fs_main";
        auto [fs_ir, fs_result] = lr::ShaderCompiler::compile(shader_compile_info);
        if (!vs_result || !fs_result) {
            LR_LOG_ERROR("Failed to initialize Cube pass! {}, {}", vs_result, fs_result);
            return false;
        }

        pipeline_info.set_shader(info.device->create_shader(lr::ShaderStageFlag::Vertex, vs_ir));
        pipeline_info.set_shader(info.device->create_shader(lr::ShaderStageFlag::Fragment, fs_ir));
        pipeline_info.set_dynamic_states(lr::DynamicState::Viewport | lr::DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});
        pipeline_info.set_vertex_layout(lr::AssetManager::VERTEX_LAYOUT);
        pipeline_info.set_rasterizer_state({ .cull_mode = lr::CullMode::Back });
        pipeline_info.set_depth_stencil_state({ .enable_depth_test = true, .enable_depth_write = true, .depth_compare_op = lr::CompareOp::LessEqual });
        pipeline_info.set_blend_attachment_all({
            .blend_enabled = true,
            .src_blend = lr::BlendFactor::SrcAlpha,
            .dst_blend = lr::BlendFactor::InvSrcAlpha,
            .src_blend_alpha = lr::BlendFactor::One,
            .dst_blend_alpha = lr::BlendFactor::InvSrcAlpha,
        });

        return true;
    }

    void execute(lr::TaskContext &tc) {
        auto &app = lr::Application::get();
        auto color_attachment_info = tc.as_color_attachment(uses.color_attachment, lr::ColorClearValue(0.01f, 0.01f, 0.01f, 1.0f));
        auto depth_attachment_info = tc.as_depth_attachment(uses.depth_attachment, lr::DepthClearValue(1.0f));

        push_constants.camera_mat = app.camera.proj_view_matrix;

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment_info,
            .depth_attachment = depth_attachment_info,
        });
        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        auto &model = app.asset_man.model_at(self.model_id);
        tc.cmd_list.set_vertex_buffer(model.vertex_buffer.value());
        tc.cmd_list.set_index_buffer(model.index_buffer.value());
        for (auto &mesh : model.meshes) {
            for (auto &primitive_index : mesh.primitive_indices) {
                auto &primitive = model.primitives[primitive_index];

                push_constants.material_buffer_id = app.asset_man.material_buffer_id;
                push_constants.material_id = primitive.material_index;
                tc.set_push_constants(push_constants);
                tc.cmd_list.draw_indexed(primitive.index_count, primitive.index_offset, static_cast<i32>(primitive.vertex_offset));
            }
        }
        tc.cmd_list.end_rendering();
    }
};

bool AdvancedApp::prepare(this AdvancedApp &self) {
    self.camera = lr::PerspectiveCamera({ 0.0f, 0.0f, -5.0f }, 60.0f, 1.6f);
    self.model_id = self.asset_man.load_model("sponza.glb").value_or(lr::ModelID::Invalid);

    self.depth_attachment_id = self.device.create_image(lr::ImageInfo{
        .usage_flags = lr::ImageUsage::DepthStencilAttachment,
        .format = lr::Format::D32_SFLOAT,
        .extent = { 1280, 720, 1 },
        .debug_name = "Depth Attachment Image",
    });

    self.depth_attachment_view_id = self.device.create_image_view(lr::ImageViewInfo{
        .image_id = self.depth_attachment_id,
        .usage_flags = lr::ImageUsage::DepthStencilAttachment,
        .type = lr::ImageViewType::View2D,
        .subresource_range = { .aspect_mask = lr::ImageAspect::Depth },
        .debug_name = "Depth Attachment ImageView",
    });
    self.depth_image_id = self.task_graph.add_image(lr::TaskPersistentImageInfo{
        .image_id = self.depth_attachment_id,
        .image_view_id = self.depth_attachment_view_id,
    });
    self.task_graph.add_task<GeometryTask>({
        .uses = {
            .color_attachment = self.swap_chain_image_id,
            .depth_attachment = self.depth_image_id,
        },
        .self = {
            .model_id = self.model_id,
        },
    });
    self.task_graph.add_task<lr::BuiltinTask::ImGuiTask>({
        .uses = {
            .attachment = self.swap_chain_image_id,
            .font = self.imgui_font_image_id,
        },
    });
    self.task_graph.present(self.swap_chain_image_id);

    return true;
}

bool AdvancedApp::update(this AdvancedApp &self, f32 delta_time) {
    self.task_graph.draw_profiler_ui();
    ImGui::SetNextWindowContentSize({ 400, 200 });
    ImGui::Begin("Camera");
    ImGui::SliderFloat3("Position", &self.camera.position.x, -10.0f, 10.0f);
    ImGui::SliderFloat("Yaw", &self.camera.yaw, -90.0f, 90.0f);
    ImGui::SliderFloat("Pitch", &self.camera.pitch, -89.0f, 89.0f);
    ImGui::End();

    return true;
}
