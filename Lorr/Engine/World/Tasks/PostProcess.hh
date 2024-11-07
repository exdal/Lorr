#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline GraphicsPipelineInfo tonemap_pipeline_info(AssetManager &asset_man, vk::Format dst_color_format) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { dst_color_format },
        .shader_module_info = {
            .module_name = "tonemap",
            .root_path = shaders_root,
            .shader_path = shaders_root / "tonemap.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .blend_attachments = { {
            .blend_enabled = false,
        } },
    };
}

struct TonemapTask {
    constexpr static std::string_view name = "Tonemap";

    struct Uses {
        Preset::ColorAttachmentRead dst_attachment = {};
        Preset::ShaderReadOnly src_image = {};
    } uses = {};

    PipelineID pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
            ImageViewID src_image_view = ImageViewID::Invalid;
            f32 avg_luminance = 3170.46533;
        };

        auto dst_attachment = tc.as_color_attachment(uses.dst_attachment);
        auto src_image_view = tc.image_view(uses.src_image.task_image_id);
        tc.set_pipeline(this->pipeline);
        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());
        tc.set_push_constants(PushConstants{
            .world_ptr = render_context.world_ptr,
            .src_image_view = src_image_view,
        });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::Tasks
