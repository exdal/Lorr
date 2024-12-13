#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline ComputePipelineInfo cloud_shape_noise_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);
    return { .shader_module_info = {
                 .module_name = "cloud.detail_noise",
                 .root_path = shaders_root,
                 .shader_path = shaders_root / "cloud" / "detail_noise.slang",
                 .entry_points = { "shape_noise_cs_main" },
             } };
}

inline ComputePipelineInfo cloud_detail_noise_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);
    return { .shader_module_info = {
                 .module_name = "cloud.detail_noise",
                 .root_path = shaders_root,
                 .shader_path = shaders_root / "cloud" / "detail_noise.slang",
                 .entry_points = { "detail_noise_cs_main" },
             } };
}

inline GraphicsPipelineInfo draw_clouds_pipeline_info(AssetManager &asset_man, vk::Format dst_color_format) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { dst_color_format },
        .shader_module_info = {
            .module_name = "cloud.draw_clouds",
            .root_path = shaders_root,
            .shader_path = shaders_root / "cloud" / "draw_clouds.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .depth_stencil_state = {
            .depth_compare_op = vk::CompareOp::LessEqual,
        },
        .blend_attachments = { {
            .blend_enabled = true,
            .src_blend = vk::BlendFactor::One,
            .dst_blend = vk::BlendFactor::SrcAlpha,
            .blend_op = vk::BlendOp::Add,
            .src_blend_alpha = vk::BlendFactor::Zero,
            .dst_blend_alpha = vk::BlendFactor::One,
            .blend_op_alpha = vk::BlendOp::Add,
        } },
    };
}

struct CloudDraw {
    constexpr static std::string_view name = "Cloud Draw";

    struct Uses {
        Preset::ColorAttachmentWrite color_attachment = {};
        Preset::ShaderReadOnly transmittance_lut_image = {};
        Preset::ShaderReadOnly aerial_perspective_lut_image = {};
        Preset::ShaderReadOnly cloud_shape_image = {};
        Preset::ShaderReadOnly cloud_detail_image = {};
    } uses = {};

    Pipeline pipeline = {};
    Sampler sampler = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            ImageViewID transmittance_lut_image = ImageViewID::Invalid;
            ImageViewID aerial_perspective_lut_image = ImageViewID::Invalid;
            SampledImage cloud_shape_image = {};
            SampledImage cloud_detail_image = {};
        };

        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        auto color_attachment = tc.as_color_attachment(uses.color_attachment);
        auto &transmittance_lut_image_use = tc.task_image_data(uses.transmittance_lut_image);
        auto &aerial_perspective_lut_image_use = tc.task_image_data(uses.aerial_perspective_lut_image);
        auto &cloud_shape_image_use = tc.task_image_data(uses.cloud_shape_image);
        auto &cloud_detail_image_use = tc.task_image_data(uses.cloud_detail_image);

        tc.set_pipeline(this->pipeline.id());

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());
        tc.set_push_constants(PushConstants{
            .world_ptr = render_context.world_ptr,
            .transmittance_lut_image = transmittance_lut_image_use.image_view_id,
            .aerial_perspective_lut_image = aerial_perspective_lut_image_use.image_view_id,
            .cloud_shape_image = { cloud_shape_image_use.image_view_id, this->sampler.id() },
            .cloud_detail_image = { cloud_detail_image_use.image_view_id, this->sampler.id() },
        });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::Tasks
