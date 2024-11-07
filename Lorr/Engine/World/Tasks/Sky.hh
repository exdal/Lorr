#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline ComputePipelineInfo sky_transmittance_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return { .shader_module_info = {
                 .module_name = "atmos.transmittance",
                 .root_path = shaders_root,
                 .shader_path = shaders_root / "atmos" / "transmittance.slang",
                 .entry_points = { "cs_main" },
             } };
}

inline ComputePipelineInfo sky_multiscatter_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return { .shader_module_info = {
                 .module_name = "atmos.ms",
                 .root_path = shaders_root,
                 .shader_path = shaders_root / "atmos" / "ms.slang",
                 .entry_points = { "cs_main" },
             } };
}

inline ComputePipelineInfo sky_aerial_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return { .shader_module_info = {
                 .module_name = "atmos.aerial_perspective",
                 .root_path = shaders_root,
                 .shader_path = shaders_root / "atmos" / "aerial_perspective.slang",
                 .entry_points = { "cs_main" },
             } };
}

inline GraphicsPipelineInfo sky_view_pipeline_info(AssetManager &asset_man, vk::Format dst_color_format) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { dst_color_format },
        .shader_module_info = {
            .module_name = "atmos.lut",
            .root_path = shaders_root,
            .shader_path = shaders_root / "atmos" / "lut.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .blend_attachments = { { .blend_enabled = false } },
    };
}

inline GraphicsPipelineInfo sky_final_pipeline_info(AssetManager &asset_man, vk::Format dst_color_format) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { dst_color_format },
        .shader_module_info = {
            .module_name = "atmos.final",
            .root_path = shaders_root,
            .shader_path = shaders_root / "atmos" / "final.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .rasterizer_state = {
            .cull_mode = vk::CullMode::Back,
        },
        .depth_stencil_state = {
            .enable_depth_test = false,
            .enable_depth_write = false,
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

inline GraphicsPipelineInfo sky_apply_aerial_pipeline_info(AssetManager &asset_man, vk::Format dst_color_format) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { dst_color_format },
        .shader_module_info = {
            .module_name = "atmos.apply_aerial_perspective",
            .root_path = shaders_root,
            .shader_path = shaders_root / "atmos" / "apply_aerial_perspective.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .depth_stencil_state = {
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

struct SkyAerialTask {
    constexpr static std::string_view name = "Sky Aerial";

    struct Uses {
        Preset::ComputeWrite aerial_lut = {};
        Preset::ShaderReadOnly transmittance_lut = {};
        Preset::ShaderReadOnly multiscatter_lut = {};
    } uses = {};

    PipelineID pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            glm::vec3 target_image_size = {};
            ImageViewID target_image_id = ImageViewID::Invalid;
            ImageViewID transmittance_image_id = ImageViewID::Invalid;
            ImageViewID multiscatter_image_id = ImageViewID::Invalid;
        };

        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        auto &aerial_use = tc.task_image_data(uses.aerial_lut);
        auto aerial_lut_info = tc.device.image(aerial_use.image_id);
        auto &transmittance_use = tc.task_image_data(uses.transmittance_lut);
        auto &multiscatter_use = tc.task_image_data(uses.multiscatter_lut);

        auto extent = aerial_lut_info.extent();
        tc.set_pipeline(this->pipeline);

        tc.set_push_constants(PushConstants{
            .world_ptr = render_context.world_ptr,
            .target_image_size = { extent.width, extent.height, extent.depth },
            .target_image_id = aerial_use.image_view_id,
            .transmittance_image_id = transmittance_use.image_view_id,
            .multiscatter_image_id = multiscatter_use.image_view_id,
        });
        tc.cmd_list.dispatch(extent.width / 16 + 1, extent.height / 16 + 1, extent.depth / 1);
    }
};

struct SkyViewTask {
    constexpr static std::string_view name = "Sky View";

    struct Uses {
        Preset::ColorAttachmentWrite sky_view_attachment = {};
        Preset::ShaderReadOnly transmittance_lut = {};
        Preset::ShaderReadOnly ms_lut = {};
    } uses = {};

    PipelineID pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            ImageViewID transmittance_image_id = ImageViewID::Invalid;
            ImageViewID ms_lut_id = ImageViewID::Invalid;
        };

        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        auto sky_view_attachment = tc.as_color_attachment(uses.sky_view_attachment, vk::ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f));
        auto &transmittance_use = tc.task_image_data(uses.transmittance_lut);
        auto &ms_use = tc.task_image_data(uses.ms_lut);

        tc.set_pipeline(this->pipeline);

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = sky_view_attachment,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());
        tc.set_push_constants(PushConstants{
            .world_ptr = render_context.world_ptr,
            .transmittance_image_id = transmittance_use.image_view_id,
            .ms_lut_id = ms_use.image_view_id,
        });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct SkyFinalTask {
    constexpr static std::string_view name = "Sky Final";

    struct Uses {
        Preset::ColorAttachmentWrite color_attachment = {};
        Preset::ShaderReadOnly sky_view_lut = {};
        Preset::ShaderReadOnly transmittance_lut = {};
    } uses = {};

    PipelineID pipeline = {};
    SamplerID sky_sampler = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            SampledImage sky_view_lut = {};
            ImageViewID transmittance_lut_id = ImageViewID::Invalid;
        };

        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        auto color_attachment = tc.as_color_attachment(uses.color_attachment, vk::ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f));
        auto &sky_view_use = tc.task_image_data(uses.sky_view_lut);
        auto &transmittance_use = tc.task_image_data(uses.transmittance_lut);

        tc.set_pipeline(this->pipeline);

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());
        tc.set_push_constants(PushConstants{
            .world_ptr = render_context.world_ptr,
            .sky_view_lut = { sky_view_use.image_view_id, sky_sampler },
            .transmittance_lut_id = transmittance_use.image_view_id,
        });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct SkyApplyAerialTask {
    constexpr static std::string_view name = "Sky Apply Aerial";

    struct Uses {
        Preset::ColorAttachmentWrite color_attachment = {};
        Preset::ShaderReadOnly depth_image = {};
        Preset::ShaderReadOnly aerial_perspective_image = {};
    } uses = {};

    PipelineID pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            ImageViewID depth_image_id = ImageViewID::Invalid;
            ImageViewID aerial_perspective_id = ImageViewID::Invalid;
        };

        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        auto color_attachment = tc.as_color_attachment(uses.color_attachment);
        auto &depth_image_use = tc.task_image_data(uses.depth_image);
        auto &aerial_perspective_image_use = tc.task_image_data(uses.aerial_perspective_image);

        tc.set_pipeline(this->pipeline);

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());
        tc.set_push_constants(PushConstants{
            .world_ptr = render_context.world_ptr,
            .depth_image_id = depth_image_use.image_view_id,
            .aerial_perspective_id = aerial_perspective_image_use.image_view_id,
        });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::Tasks
