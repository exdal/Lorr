#pragma once

#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline auto transmittance_lut_dispatch(Device &device, WorldRenderContext &render_context, const fs::path &shaders_root) -> void {
    ZoneScoped;

    // clang-format off
    auto sky_transmittance_pipeline = Pipeline::create(device, {
        .module_name = "atmos.transmittance",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "transmittance.slang",
        .entry_points = { "cs_main" },
    }).value();
    // clang-format on

    struct PushConstants {
        u64 world_ptr = 0;
        ImageViewID image_view_id = ImageViewID::Invalid;
        glm::vec2 image_size = {};
        u32 pad = 0;
    };

    auto pass = vuk::make_pass(
        "transmittance lut",
        [&, pipeline_id = sky_transmittance_pipeline.id()](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeRW) dst) {
            auto &target_image_view = render_context.sky_transmittance_lut_view;
            auto pipeline = device.pipeline(pipeline_id);
            auto image_size = glm::vec2(target_image_view.extent().width, target_image_view.extent().height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(*pipeline)
                .bind_persistent(0, device.bindless_descriptor_set())
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = render_context.world_ptr,
                        .image_view_id = target_image_view.id(),
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return dst;
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto &target_image_view = render_context.sky_transmittance_lut_view;
    auto attachment_info = target_image_view.get_attachment(device, vuk::ImageUsageFlagBits::eStorage);
    auto attachment = vuk::declare_ia("transmittance lut", attachment_info);

    auto result = pass(std::move(attachment));
    result.release(vuk::Access::eComputeSampled, vuk::DomainFlagBits::eGraphicsQueue);
    device.transfer_man().wait_on(std::move(result));
}

inline auto multiscatter_lut_dispatch(Device &device, WorldRenderContext &render_context, const fs::path &shaders_root) -> void {
    ZoneScoped;

    // clang-format off
    auto sky_transmittance_pipeline = Pipeline::create(device, {
        .module_name = "atmos.ms",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "ms.slang",
        .entry_points = { "cs_main" },
    }).value();
    // clang-format on

    struct PushConstants {
        u64 world_ptr = 0;
        ImageViewID transmittance_lut_view_id = ImageViewID::Invalid;
        ImageViewID image_view_id = ImageViewID::Invalid;
        glm::vec2 image_size = {};
        u32 pad = 0;
    };

    auto pass = vuk::make_pass(
        "multiscatter lut",
        [&, pipeline_id = sky_transmittance_pipeline.id()](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeSampled) transmittance_lut, VUK_IA(vuk::Access::eComputeRW) dst) {
            auto transmittance_lut_view_id = render_context.sky_transmittance_lut_view.id();
            auto &target_image_view = render_context.sky_multiscatter_lut_view;
            auto pipeline = device.pipeline(pipeline_id);
            auto image_size = glm::vec2(target_image_view.extent().width, target_image_view.extent().height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(*pipeline)
                .bind_persistent(0, device.bindless_descriptor_set())
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = render_context.world_ptr,
                        .transmittance_lut_view_id = transmittance_lut_view_id,
                        .image_view_id = target_image_view.id(),
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst);
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto &target_image_view = render_context.sky_multiscatter_lut_view;
    auto multiscatter_attachment_info = target_image_view.get_attachment(device, vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_attachment = vuk::declare_ia("multiscatter lut", multiscatter_attachment_info);

    auto &transmittance_lut_view = render_context.sky_transmittance_lut_view;
    auto transmittance_attachment_info = transmittance_lut_view.get_attachment(device, vuk::ImageUsageFlagBits::eSampled);
    auto transmittance_attachment = vuk::acquire_ia("transmittance lut", transmittance_attachment_info, vuk::Access::eComputeSampled);

    auto [transmittance_lut_fut, multiscatter_lut_fut] = pass(std::move(transmittance_attachment), std::move(multiscatter_attachment));
    transmittance_lut_fut.release(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
    multiscatter_lut_fut.release(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
    device.transfer_man().wait_on(std::move(transmittance_lut_fut));
    device.transfer_man().wait_on(std::move(multiscatter_lut_fut));
}

inline auto sky_view_draw(Device &device, WorldRenderContext &render_context) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    struct PushConstants {
        u64 world_ptr = 0;
        ImageViewID transmittance_view_id = ImageViewID::Invalid;
        ImageViewID ms_view_id = ImageViewID::Invalid;
    };

    auto pass = vuk::make_pass(
        "sky view",
        [&](vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut) {
            auto pipeline = device.pipeline(render_context.sky_view_pipeline.id());
            cmd_list  //
                .bind_graphics_pipeline(*pipeline)
                .bind_persistent(0, device.bindless_descriptor_set())
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(
                    vuk::ShaderStageFlagBits::eFragment,
                    0,
                    PushConstants{
                        .world_ptr = render_context.world_ptr,
                        .transmittance_view_id = render_context.sky_transmittance_lut_view.id(),
                        .ms_view_id = render_context.sky_multiscatter_lut_view.id(),
                    })
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut);
        });

    auto &sky_view_lut_view = render_context.sky_view_lut_view;
    auto sky_view_lut_attachment_info = sky_view_lut_view.get_attachment(device, vuk::ImageUsageFlagBits::eColorAttachment);
    auto sky_view_lut_attachment = vuk::declare_ia("sky view", sky_view_lut_attachment_info);

    auto &transmittance_lut_view = render_context.sky_transmittance_lut_view;
    auto transmittance_attachment_info = transmittance_lut_view.get_attachment(device, vuk::ImageUsageFlagBits::eSampled);
    auto transmittance_attachment = vuk::acquire_ia("transmittance lut", transmittance_attachment_info, vuk::Access::eFragmentSampled);

    auto &multiscatter_lut_view = render_context.sky_multiscatter_lut_view;
    auto multiscatter_attachment_info = multiscatter_lut_view.get_attachment(device, vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_attachment = vuk::acquire_ia("multiscatter lut", multiscatter_attachment_info, vuk::Access::eFragmentSampled);

    sky_view_lut_attachment = vuk::clear_image(std::move(sky_view_lut_attachment), vuk::Black<f32>);

    auto [result, transmittance_fut, ms_fut] =
        pass(std::move(sky_view_lut_attachment), std::move(transmittance_attachment), std::move(multiscatter_attachment));
    // result.release(vuk::Access::eFragmentSampled);

    return result;
}

}  // namespace lr::Tasks
