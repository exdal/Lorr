#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline GraphicsPipelineInfo grid_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    return {
        .color_attachment_formats = { vk::Format::R8G8B8A8_SRGB },
        .depth_attachment_format = vk::Format::D32_SFLOAT,
        .shader_module_info = {
            .module_name = "editor.grid",
            .root_path = shaders_root,
            .shader_path = shaders_root / "editor" / "grid.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .rasterizer_state = {.cull_mode = vk::CullMode::None },
        .depth_stencil_state = {
            .enable_depth_test = true,
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

struct GridTask {
    constexpr static std::string_view name = "Grid";

    struct Uses {
        Preset::ColorAttachmentRead color_attachment = {};
        Preset::DepthAttachmentRead depth_attachment = {};
    } uses = {};

    PipelineID pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            u64 world_ptr = 0;
        };

        auto color_attachment = tc.as_color_attachment(uses.color_attachment);
        auto depth_attachment = tc.as_depth_attachment(uses.depth_attachment);
        tc.set_pipeline(this->pipeline);
        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment,
            .depth_attachment = depth_attachment,
        });

        tc.cmd_list.set_viewport(tc.pass_viewport());
        tc.cmd_list.set_scissors(tc.pass_rect());
        tc.set_push_constants(PushConstants{ .world_ptr = render_context.world_ptr });
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::Tasks
