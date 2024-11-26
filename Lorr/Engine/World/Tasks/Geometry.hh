#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline GraphicsPipelineInfo geometry_pipeline_info(AssetManager &asset_man, vk::Format dst_color_format, vk::Format dst_depth_format) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { dst_color_format },
        .depth_attachment_format = dst_depth_format,
        .shader_module_info = {
            .module_name = "model",
            .root_path = shaders_root,
            .shader_path = shaders_root / "model.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .vertex_attrib_infos = {
            { .format = vk::Format::R32G32B32_SFLOAT, .location = 0, .offset = offsetof(Model::Vertex, position) },
            { .format = vk::Format::R32_SFLOAT, .location = 1, .offset = offsetof(Model::Vertex, uv_x) },
            { .format = vk::Format::R32G32B32_SFLOAT, .location = 2, .offset = offsetof(Model::Vertex, normal) },
            { .format = vk::Format::R32_SFLOAT, .location = 3, .offset = offsetof(Model::Vertex, uv_y) },
        },
        .rasterizer_state = {
            .cull_mode = vk::CullMode::Back,
        },
        .depth_stencil_state = {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .depth_compare_op = vk::CompareOp::LessEqual,
        },
        .blend_attachments = { {
            .blend_enabled = true,
            .src_blend = vk::BlendFactor::SrcAlpha,
            .dst_blend = vk::BlendFactor::InvSrcAlpha,
            .blend_op = vk::BlendOp::Add,
            .src_blend_alpha = vk::BlendFactor::One,
            .dst_blend_alpha = vk::BlendFactor::InvSrcAlpha,
            .blend_op_alpha = vk::BlendOp::Add,
        } },
    };
}

struct GeometryTask {
    constexpr static std::string_view name = "Geometry";

    struct Uses {
        Preset::ColorAttachmentWrite color_attachment = {};
        Preset::DepthAttachmentWrite depth_attachment = {};
        Preset::ShaderReadOnly transmittance_image = {};
    } uses = {};

    Pipeline pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            ImageViewID transmittance_image = ImageViewID::Invalid;
            u32 model_transform_index = ~0_u32;
            u32 material_index = 0;
            u32 pad = 0;
        };

        auto &render_context = tc.exec_data_as<WorldRenderContext>();

        auto color_attachment_info = tc.as_color_attachment(uses.color_attachment);
        auto depth_attachment_info = tc.as_depth_attachment(uses.depth_attachment, vk::DepthClearValue(1.0f));
        auto transmittance_image_view = tc.image_view(uses.transmittance_image.task_image_id);

        tc.set_pipeline(this->pipeline.id());

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment_info,
            .depth_attachment = depth_attachment_info,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());

        for (auto &model : render_context.models) {
            tc.cmd_list.set_vertex_buffer(model.vertex_bufffer_id);
            tc.cmd_list.set_index_buffer(model.index_buffer_id);
            for (auto &mesh : model.meshes) {
                for (auto &primitive_index : mesh.primitive_indices) {
                    auto &primitive = model.primitives[primitive_index];
                    tc.set_push_constants(PushConstants{
                        .world_ptr = render_context.world_ptr,
                        .transmittance_image = transmittance_image_view,
                        .model_transform_index = 0,
                        .material_index = primitive.material_index,
                    });
                    tc.cmd_list.draw_indexed(primitive.index_count, primitive.index_offset, static_cast<i32>(primitive.vertex_offset));
                }
            }
        }

        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::Tasks
