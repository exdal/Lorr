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

inline GraphicsPipelineInfo sky_view_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { vk::Format::R32G32B32A32_SFLOAT },
        .shader_module_info = {
            .module_name = "lut",
            .root_path = shaders_root,
            .shader_path = shaders_root / "atmos" / "lut.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .blend_attachments = { { .blend_enabled = false } },
    };
}

inline GraphicsPipelineInfo sky_final_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { vk::Format::R8G8B8A8_SRGB },
        .shader_module_info = {
            .module_name = "lut",
            .root_path = shaders_root,
            .shader_path = shaders_root / "atmos" / "final.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .blend_attachments = { { .blend_enabled = false } },
    };
}

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
            ImageViewID sky_view_image_id = ImageViewID::Invalid;
            SamplerID sky_view_sampler_id = SamplerID::Invalid;
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
            .sky_view_image_id = sky_view_use.image_view_id,
            .sky_view_sampler_id = sky_sampler,
            .transmittance_lut_id = transmittance_use.image_view_id,
        });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::Tasks
