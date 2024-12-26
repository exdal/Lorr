#pragma once

#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline auto grid_draw(Device &device, vuk::Value<vuk::ImageAttachment> &&target_attachment, WorldRenderContext &render_context)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    struct PushConstants {
        u64 world_ptr = 0;
    };

    auto pass = vuk::make_pass("editor grid", [&](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst) {
        auto pipeline = device.pipeline(render_context.grid_pipeline.id());
        cmd_list  //
            .bind_graphics_pipeline(*pipeline)
            .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
            .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
            .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
            .set_viewport(0, vuk::Rect2D::framebuffer())
            .set_scissor(0, vuk::Rect2D::framebuffer())
            .push_constants(
                vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment,
                0,
                PushConstants{ .world_ptr = render_context.world_ptr })
            .draw(3, 1, 0, 0);
        return dst;
    });

    return pass(std::move(target_attachment));
}

}  // namespace lr::Tasks
