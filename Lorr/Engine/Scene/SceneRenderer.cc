#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Core/Application.hh"

#include <imgui.h>

namespace lr {
auto SceneRenderer::init(this SceneRenderer &self, Device *device) -> bool {
    self.device = device;

    self.setup_persistent_resources();
    return true;
}

auto SceneRenderer::setup_persistent_resources(this SceneRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &transfer_man = app.device.transfer_man();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    // clang-format off
    self.linear_sampler = Sampler::create(
        *self.device,
        vuk::Filter::eLinear,
        vuk::Filter::eLinear,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::CompareOp::eNever)
        .value();
    self.nearest_sampler = Sampler::create(
        *self.device,
        vuk::Filter::eNearest,
        vuk::Filter::eNearest,
        vuk::SamplerMipmapMode::eNearest,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::CompareOp::eNever)
        .value();

    //  ── GRID ────────────────────────────────────────────────────────────
    self.grid_pipeline = Pipeline::create(*self.device, {
        .module_name = "editor.grid",
        .root_path = shaders_root,
        .shader_path = shaders_root / "editor" / "grid.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    //  ── SKY ─────────────────────────────────────────────────────────────
    self.sky_transmittance_lut = Image::create(
        *self.device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(256, 64, 1))
        .value();
    self.sky_transmittance_lut_view = ImageView::create(
        *self.device,
        self.sky_transmittance_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { .aspectMask = vuk::ImageAspectFlagBits::eColor })
        .value();
    self.sky_transmittance_pipeline = Pipeline::create(*self.device, {
        .module_name = "atmos.transmittance",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "transmittance.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.sky_multiscatter_lut = Image::create(
        *self.device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(32, 32, 1))
        .value();
    self.sky_multiscatter_lut_view = ImageView::create(
        *self.device,
        self.sky_multiscatter_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { .aspectMask = vuk::ImageAspectFlagBits::eColor })
        .value();
    self.sky_multiscatter_pipeline = Pipeline::create(*self.device, {
        .module_name = "atmos.ms",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "ms.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.sky_view_pipeline = Pipeline::create(*self.device, {
        .module_name = "atmos.lut",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "lut.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    self.sky_aerial_perspective_pipeline = Pipeline::create(*self.device, {
        .module_name = "atmos.aerial_perspective",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "aerial_perspective.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.sky_final_pipeline = Pipeline::create(*self.device, {
        .module_name = "atmos.final",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "final.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    self.vis_triangle_id_pipeline = Pipeline::create(*self.device, {
        //.definitions = { {"LR_BINDLESS_PIPELINE", "1"} },
        .module_name = "vis.triangle_id",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "triangle_id.slang",
        .entry_points = { "vs_main", "fs_main" },
        //.bindless_pipeline = true,
    }).value();
    self.tonemap_pipeline = Pipeline::create(*self.device, {
        .module_name = "tonemap",
        .root_path = shaders_root,
        .shader_path = shaders_root / "tonemap.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    // clang-format on

    //  ── DEFINE PASSES ───────────────────────────────────────────────────
    auto transmittance_lut_pass = vuk::make_pass(
        "transmittance lut",
        [&pipeline = *self.device->pipeline(self.sky_transmittance_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeRW) dst, VUK_BA(vuk::Access::eComputeRead) atmos) {
            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, dst)
                .bind_buffer(0, 1, atmos)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, image_size)
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(dst, atmos);
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&pipeline = *self.device->pipeline(self.sky_multiscatter_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
            VUK_IA(vuk::Access::eComputeRW) dst,
            VUK_BA(vuk::Access::eComputeRead) atmos,
            VUK_BA(vuk::Access::eComputeRead) sun) {
            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, dst)
                .bind_buffer(0, 3, atmos)
                .bind_buffer(0, 4, sun)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, image_size)
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst);
        },
        vuk::DomainFlagBits::eGraphicsQueue);
    //   ──────────────────────────────────────────────────────────────────────

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky_transmittance_lut", vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.discard(*self.device, "sky_multiscatter_lut", vuk::ImageUsageFlagBits::eStorage);
    auto temp_atmos = transfer_man.scratch_buffer(GPUAtmosphereData{});
    auto temp_sun = transfer_man.scratch_buffer(GPUSunData{});

    std::tie(transmittance_lut_attachment, temp_atmos) =
        transmittance_lut_pass(std::move(transmittance_lut_attachment), std::move(temp_atmos));
    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment) = multiscatter_lut_pass(
        std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment), std::move(temp_atmos), std::move(temp_sun));

    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));
}

auto SceneRenderer::render(this SceneRenderer &self, const SceneRenderInfo &info) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();
    auto final_attachment = vuk::declare_ia(
        "final", { .extent = info.extent, .format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia(
        "depth", { .extent = info.extent, .format = vuk::Format::eD32Sfloat, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::ClearDepth(1.0));

    LS_EXPECT(info.camera_info.has_value());
    auto camera_buffer = transfer_man.scratch_buffer(info.camera_info.value());

    //  ── SKY VIEW LUT ────────────────────────────────────────────────────
    auto sky_view_pass = vuk::make_pass(
        "sky view",
        [&pipeline = *self.device->pipeline(self.sky_view_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut,
            VUK_BA(vuk::Access::eFragmentRead) atmos,
            VUK_BA(vuk::Access::eFragmentRead) sun,
            VUK_BA(vuk::Access::eFragmentRead) camera) {
            vuk::SamplerCreateInfo sampler_info = {
                .magFilter = vuk::Filter::eLinear,
                .minFilter = vuk::Filter::eLinear,
                .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
            };

            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, multiscatter_lut)
                .bind_buffer(0, 3, atmos)
                .bind_buffer(0, 4, sun)
                .bind_buffer(0, 5, camera)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut, atmos, sun, camera);
        });

    auto transmittance_lut_attachment = self.sky_transmittance_lut_view.acquire(
        *self.device, "transmittance_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
    auto multiscatter_lut_attachment = self.sky_multiscatter_lut_view.acquire(
        *self.device, "multiscatter_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

    auto sky_view_lut_attachment = vuk::declare_ia(
        "sky_view_lut", { .extent = { .width = 200, .height = 100, .depth = 1 }, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    sky_view_lut_attachment.same_format_as(final_attachment);
    sky_view_lut_attachment = vuk::clear_image(std::move(sky_view_lut_attachment), vuk::Black<f32>);

    const auto rendering_atmos = info.atmosphere.has_value() && info.sun.has_value();
    vuk::Value<vuk::Buffer> atmos_buffer = {};
    vuk::Value<vuk::Buffer> sun_buffer = {};
    if (info.atmosphere.has_value()) {
        atmos_buffer = transfer_man.scratch_buffer(info.atmosphere.value_or(GPUAtmosphereData{}));
    }

    if (info.sun.has_value()) {
        sun_buffer = transfer_man.scratch_buffer(info.sun.value_or(GPUSunData{}));
    }

    std::tie(sky_view_lut_attachment, transmittance_lut_attachment, multiscatter_lut_attachment, atmos_buffer, sun_buffer, camera_buffer) =
        sky_view_pass(
            std::move(sky_view_lut_attachment),
            std::move(transmittance_lut_attachment),
            std::move(multiscatter_lut_attachment),
            std::move(atmos_buffer),
            std::move(sun_buffer),
            std::move(camera_buffer));

    //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
    auto sky_aerial_perspective_pass = vuk::make_pass(
        "sky aerial perspective",
        [&pipeline = *self.device->pipeline(self.sky_aerial_perspective_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeWrite) dst,
            VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
            VUK_IA(vuk::Access::eComputeSampled) multiscatter_lut,
            VUK_BA(vuk::Access::eComputeRead) atmos,
            VUK_BA(vuk::Access::eComputeRead) sun,
            VUK_BA(vuk::Access::eComputeRead) camera) {
            vuk::SamplerCreateInfo sampler_info = {
                .magFilter = vuk::Filter::eLinear,
                .minFilter = vuk::Filter::eLinear,
                .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
            };

            auto image_size = glm::vec3(dst->extent.width, dst->extent.height, dst->extent.height);
            auto groups = glm::uvec2(image_size) / glm::uvec2(16);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, dst)
                .bind_image(0, 2, transmittance_lut)
                .bind_image(0, 3, multiscatter_lut)
                .bind_buffer(0, 4, atmos)
                .bind_buffer(0, 5, sun)
                .bind_buffer(0, 6, camera)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, image_size)
                .dispatch(groups.x, groups.y, static_cast<u32>(image_size.z));
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut, atmos, sun, camera);
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto sky_aerial_perspective_attachment = vuk::declare_ia(
        "sky_aerial_perspective",
        { .image_type = vuk::ImageType::e3D,
          .extent = { .width = 32, .height = 32, .depth = 32 },
          .sample_count = vuk::Samples::e1,
          .view_type = vuk::ImageViewType::e3D,
          .level_count = 1,
          .layer_count = 1 });
    sky_aerial_perspective_attachment.same_format_as(sky_view_lut_attachment);
    std::tie(
        sky_aerial_perspective_attachment,
        transmittance_lut_attachment,
        multiscatter_lut_attachment,
        atmos_buffer,
        sun_buffer,
        camera_buffer) =
        sky_aerial_perspective_pass(
            std::move(sky_aerial_perspective_attachment),
            std::move(transmittance_lut_attachment),
            std::move(multiscatter_lut_attachment),
            std::move(atmos_buffer),
            std::move(sun_buffer),
            std::move(camera_buffer));

    //  ── SKY FINAL ───────────────────────────────────────────────────────
    auto sky_final_pass = vuk::make_pass(
        "sky final",
        [&pipeline = *self.device->pipeline(self.sky_final_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) sky_view_lut,
            VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_BA(vuk::Access::eFragmentRead) atmos,
            VUK_BA(vuk::Access::eFragmentRead) sun,
            VUK_BA(vuk::Access::eFragmentRead) camera) {
            vuk::SamplerCreateInfo sampler_info = {
                .magFilter = vuk::Filter::eLinear,
                .minFilter = vuk::Filter::eLinear,
                .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
            };

            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, sky_view_lut)
                .bind_buffer(0, 3, atmos)
                .bind_buffer(0, 4, sun)
                .bind_buffer(0, 5, camera)
                .set_rasterization({})
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, sky_view_lut, aerial_perspective_lut, transmittance_lut, atmos, sun, camera);
        });

    if (rendering_atmos) {
        std::tie(
            final_attachment,
            sky_view_lut_attachment,
            sky_aerial_perspective_attachment,
            transmittance_lut_attachment,
            atmos_buffer,
            sun_buffer,
            camera_buffer) =
            sky_final_pass(
                std::move(final_attachment),
                std::move(sky_view_lut_attachment),
                std::move(sky_aerial_perspective_attachment),
                std::move(transmittance_lut_attachment),
                std::move(atmos_buffer),
                std::move(sun_buffer),
                std::move(camera_buffer));
    }

    //  ── VIS TRI ─────────────────────────────────────────────────────────
    for (const auto &model : self.rendering_models) {
        auto vis_triangle_id_pass = vuk::make_pass(
            "vis triangle",
            [&pipeline = *self.device->pipeline(self.vis_triangle_id_pipeline.id()), &device = self.device, model](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eColorWrite) dst,
                VUK_IA(vuk::Access::eDepthStencilRW) dst_depth,
                VUK_BA(vuk::Access::eVertexRead) camera,
                VUK_BA(vuk::Access::eVertexRead) mesh_instances) {
                cmd_list  //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                    .set_depth_stencil(
                        { .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, mesh_instances)
                    .bind_buffer(0, 2, *device->buffer(model.vertex_buffer.id()))
                    .bind_buffer(0, 3, *device->buffer(model.reordered_index_buffer.id()))
                    .bind_index_buffer(*device->buffer(model.provoked_index_buffer.id()), vuk::IndexType::eUint32);

                for (usize node_index = 0; node_index < model.nodes.size(); node_index++) {
                    const auto &node = model.nodes[node_index];
                    cmd_list  //
                        .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, node_index);
                    for (const auto mesh_index : node.mesh_indices) {
                        const auto &mesh = model.meshes[mesh_index];
                        for (const auto &meshlet_index : mesh.meshlet_indices) {
                            const auto &meshlet = model.meshlets[meshlet_index];
                            cmd_list  //
                                .draw_indexed(meshlet.index_count, 1, meshlet.index_offset, static_cast<i32>(meshlet.vertex_offset), 0);
                        }
                    }
                }

                return std::make_tuple(dst, dst_depth, camera, mesh_instances);
            });

        std::tie(final_attachment, depth_attachment, camera_buffer) =
            vis_triangle_id_pass(std::move(final_attachment), std::move(depth_attachment));
    }

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [&pipeline = *self.device->pipeline(self.grid_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eDepthStencilRW) depth,
            VUK_BA(vuk::Access::eVertexRead | vuk::Access::eFragmentRead) camera) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_buffer(0, 0, camera)
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth, camera);
        });

    std::tie(final_attachment, depth_attachment, camera_buffer) =
        editor_grid_pass(std::move(final_attachment), std::move(depth_attachment), std::move(camera_buffer));

    //  ── TONEMAP ─────────────────────────────────────────────────────────
    auto tonemap_pass = vuk::make_pass(
        "tonemap",
        [&pipeline = *self.device->pipeline(self.tonemap_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst, VUK_IA(vuk::Access::eFragmentSampled) src) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, src)
                .set_rasterization({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, src);
        });

    auto composition_attachment =
        vuk::declare_ia("result", { .extent = info.extent, .format = info.format, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    std::tie(composition_attachment, final_attachment) = tonemap_pass(std::move(composition_attachment), std::move(final_attachment));
    return composition_attachment;
}

auto SceneRenderer::draw_profiler_ui(this SceneRenderer &) -> void {
    ZoneScoped;
}

}  // namespace lr
