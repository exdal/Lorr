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
        .entry_points = { "cs_main" },
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
    //  ── CLOUD ───────────────────────────────────────────────────────────
    self.cloud_noise_pipeline = Pipeline::create(*self.device, {
        .module_name = "cloud.noise",
        .root_path = shaders_root,
        .shader_path = shaders_root / "cloud" / "noise.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.cloud_shape_noise_lut = Image::create(
        *self.device,
        vuk::Format::eR8G8B8A8Unorm,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e3D,
        vuk::Extent3D(128, 128, 128))
        .value();
    self.cloud_shape_noise_lut_view = ImageView::create(
        *self.device,
        self.cloud_shape_noise_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e3D,
        { .aspectMask = vuk::ImageAspectFlagBits::eColor })
        .value();
    self.cloud_detail_noise_lut = Image::create(
        *self.device,
        vuk::Format::eR8G8B8A8Unorm,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e3D,
        vuk::Extent3D(128, 128, 128))
        .value();
    self.cloud_detail_noise_lut_view = ImageView::create(
        *self.device,
        self.cloud_detail_noise_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e3D,
        { .aspectMask = vuk::ImageAspectFlagBits::eColor })
        .value();
    self.cloud_apply_pipeline = Pipeline::create(*self.device, {
        .module_name = "cloud.apply_clouds",
        .root_path = shaders_root,
        .shader_path = shaders_root / "cloud" / "apply_clouds.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    //  ── VISBUFFER ───────────────────────────────────────────────────────
    self.vis_triangle_id_pipeline = Pipeline::create(*self.device, {
        .module_name = "vis.triangle_id",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "triangle_id.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    //  ── POST PROCESS ────────────────────────────────────────────────────
    self.tonemap_pipeline = Pipeline::create(*self.device, {
        .module_name = "tonemap",
        .root_path = shaders_root,
        .shader_path = shaders_root / "tonemap.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    // clang-format on

    //  ── SKY LUTS ────────────────────────────────────────────────────────
    auto transmittance_lut_pass = vuk::make_pass(
        "transmittance lut",
        [&pipeline = *self.device->pipeline(self.sky_transmittance_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeRW) dst, VUK_BA(vuk::Access::eComputeRead) atmos) {
            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, dst)
                .bind_buffer(0, 1, atmos)
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
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, dst)
                .bind_buffer(0, 3, atmos)
                .bind_buffer(0, 4, sun)
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst);
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky_transmittance_lut", vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.discard(*self.device, "sky_multiscatter_lut", vuk::ImageUsageFlagBits::eStorage);
    auto temp_atmos = transfer_man.scratch_buffer(GPUAtmosphereData{
        .transmittance_lut_size = self.sky_transmittance_lut_view.extent(),
        .multiscattering_lut_size = self.sky_multiscatter_lut_view.extent(),
    });
    auto temp_sun = transfer_man.scratch_buffer(GPUSunData{});

    std::tie(transmittance_lut_attachment, temp_atmos) =
        transmittance_lut_pass(std::move(transmittance_lut_attachment), std::move(temp_atmos));
    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment) = multiscatter_lut_pass(
        std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment), std::move(temp_atmos), std::move(temp_sun));

    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));

    //  ── CLOUD LUTS ──────────────────────────────────────────────────────
    auto temp_clouds = transfer_man.scratch_buffer(GPUCloudsData{
        .noise_lut_size = self.cloud_shape_noise_lut_view.extent(),
    });
    auto cloud_noise_lut_pass = vuk::make_pass(
        "cloud noise pass",
        [&pipeline = *self.device->pipeline(self.cloud_noise_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeRW) shape_noise_lut,
            VUK_IA(vuk::Access::eComputeRW) detail_noise_lut,
            VUK_BA(vuk::Access::eComputeRead) clouds) {
            auto image_size = glm::vec2(shape_noise_lut->extent.width, shape_noise_lut->extent.height);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, shape_noise_lut)
                .bind_image(0, 1, detail_noise_lut)
                .bind_buffer(0, 2, clouds)
                .dispatch(groups.x, groups.y, shape_noise_lut->extent.depth);
            return std::make_tuple(shape_noise_lut, detail_noise_lut, clouds);
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto cloud_shape_noise_lut_attachment =
        self.cloud_shape_noise_lut_view.discard(*self.device, "cloud shape lut", vuk::ImageUsageFlagBits::eStorage);
    auto cloud_detail_noise_lut_attachment =
        self.cloud_detail_noise_lut_view.discard(*self.device, "cloud detail lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(cloud_shape_noise_lut_attachment, cloud_detail_noise_lut_attachment, temp_clouds) = cloud_noise_lut_pass(
        std::move(cloud_shape_noise_lut_attachment), std::move(cloud_detail_noise_lut_attachment), std::move(temp_clouds));
    transfer_man.wait_on(std::move(cloud_shape_noise_lut_attachment));
    transfer_man.wait_on(std::move(cloud_detail_noise_lut_attachment));
}

auto SceneRenderer::update_entity_transform(this SceneRenderer &self, GPUEntityID entity_id, GPUEntityTransform transform) -> void {
    ZoneScoped;

    u64 target_offset = static_cast<u64>(SlotMap_decode_id(entity_id).index) * sizeof(GPUEntityTransform);
    // Check if we have enough storage capacity
    if (self.gpu_entities_buffer.data_size() <= target_offset) {
        if (self.gpu_entities_buffer.id() != BufferID::Invalid) {
            // Device wait here is important, do not remove it. Why?
            // We are using ONE transform buffer for all frames, if
            // this buffer gets destroyed in current frame, previous
            // rendering frame buffer will get corrupt and crash GPU.
            self.device->wait();
            self.device->destroy(self.gpu_entities_buffer.id());
        }

        auto new_buffer_size = ls::max(target_offset * 2, sizeof(GPUEntityTransform) * 64);
        self.gpu_entities_buffer = Buffer::create(*self.device, new_buffer_size, vuk::MemoryUsage::eGPUonly).value();
    }

    auto &transfer_man = self.device->transfer_man();
    transfer_man.upload_staging(ls::span(&transform, 1), self.gpu_entities_buffer, target_offset);
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
            VUK_IA(vuk::Access::eComputeRW) dst,
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

            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, dst)
                .bind_image(0, 2, transmittance_lut)
                .bind_image(0, 3, multiscatter_lut)
                .bind_buffer(0, 4, atmos)
                .bind_buffer(0, 5, sun)
                .bind_buffer(0, 6, camera)
                .dispatch(groups.x, groups.y, 1);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut, atmos, sun, camera);
        });

    auto transmittance_lut_attachment = self.sky_transmittance_lut_view.acquire(
        *self.device, "transmittance_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
    auto multiscatter_lut_attachment = self.sky_multiscatter_lut_view.acquire(
        *self.device, "multiscatter_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

    auto sky_view_lut_attachment = vuk::declare_ia(
        "sky_view_lut",
        { .image_type = vuk::ImageType::e2D,
          .extent = self.sky_view_lut_extent,
          .sample_count = vuk::Samples::e1,
          .view_type = vuk::ImageViewType::e2D,
          .level_count = 1,
          .layer_count = 1 });
    sky_view_lut_attachment.same_format_as(final_attachment);

    const auto rendering_atmos = info.atmosphere.has_value() && info.sun.has_value();
    vuk::Value<vuk::Buffer> atmos_buffer = {};
    vuk::Value<vuk::Buffer> sun_buffer = {};
    if (info.atmosphere.has_value()) {
        auto atmos_info = info.atmosphere.value();
        atmos_info.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
        atmos_info.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
        atmos_info.sky_view_lut_size = self.sky_view_lut_extent;
        atmos_info.aerial_perspective_lut_size = self.sky_aerial_perspective_lut_extent;

        atmos_buffer = transfer_man.scratch_buffer(atmos_info);
    }

    if (info.sun.has_value()) {
        sun_buffer = transfer_man.scratch_buffer(info.sun.value_or(GPUSunData{}));
    }

    if (rendering_atmos) {
        std::tie(
            sky_view_lut_attachment, transmittance_lut_attachment, multiscatter_lut_attachment, atmos_buffer, sun_buffer, camera_buffer) =
            sky_view_pass(
                std::move(sky_view_lut_attachment),
                std::move(transmittance_lut_attachment),
                std::move(multiscatter_lut_attachment),
                std::move(atmos_buffer),
                std::move(sun_buffer),
                std::move(camera_buffer));
    }

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

            auto image_size = glm::vec3(dst->extent.width, dst->extent.height, dst->extent.depth);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, dst)
                .bind_image(0, 2, transmittance_lut)
                .bind_image(0, 3, multiscatter_lut)
                .bind_buffer(0, 4, atmos)
                .bind_buffer(0, 5, sun)
                .bind_buffer(0, 6, camera)
                .dispatch(groups.x, groups.y, static_cast<u32>(image_size.z));
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut, atmos, sun, camera);
        });

    auto sky_aerial_perspective_attachment = vuk::declare_ia(
        "sky_aerial_perspective",
        { .image_type = vuk::ImageType::e3D,
          .extent = self.sky_aerial_perspective_lut_extent,
          .sample_count = vuk::Samples::e1,
          .view_type = vuk::ImageViewType::e3D,
          .level_count = 1,
          .layer_count = 1 });
    sky_aerial_perspective_attachment.same_format_as(sky_view_lut_attachment);

    if (rendering_atmos) {
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
    }

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

    //  ── CLOUDS ──────────────────────────────────────────────────────────
    auto cloud_apply_pass = vuk::make_pass(
        "cloud apply",
        [&pipeline = *self.device->pipeline(self.cloud_apply_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorRW) dst,
            VUK_IA(vuk::Access::eFragmentSampled) shape_noise_lut,
            VUK_IA(vuk::Access::eFragmentSampled) detail_noise_lut,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
            VUK_BA(vuk::Access::eFragmentRead) sun,
            VUK_BA(vuk::Access::eFragmentRead) atmos,
            VUK_BA(vuk::Access::eFragmentRead) clouds,
            VUK_BA(vuk::Access::eFragmentRead) camera) {
            vuk::SamplerCreateInfo linear_repeat_sampler = {
                .magFilter = vuk::Filter::eLinear,
                .minFilter = vuk::Filter::eLinear,
                .addressModeU = vuk::SamplerAddressMode::eRepeat,
                .addressModeV = vuk::SamplerAddressMode::eRepeat,
                .addressModeW = vuk::SamplerAddressMode::eRepeat,
            };
            vuk::SamplerCreateInfo linear_clamp_to_ege_sampler = {
                .magFilter = vuk::Filter::eLinear,
                .minFilter = vuk::Filter::eLinear,
                .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
            };

            vuk::PipelineColorBlendAttachmentState blend_info = {
                .blendEnable = true,
                .srcColorBlendFactor = vuk::BlendFactor::eOne,
                .dstColorBlendFactor = vuk::BlendFactor::eSrcAlpha,
                .colorBlendOp = vuk::BlendOp::eAdd,
                .srcAlphaBlendFactor = vuk::BlendFactor::eZero,
                .dstAlphaBlendFactor = vuk::BlendFactor::eOne,
                .alphaBlendOp = vuk::BlendOp::eAdd,
            };

            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, linear_repeat_sampler)
                .bind_sampler(0, 1, linear_clamp_to_ege_sampler)
                .bind_image(0, 2, shape_noise_lut)
                .bind_image(0, 3, detail_noise_lut)
                .bind_image(0, 4, transmittance_lut)
                .bind_image(0, 5, aerial_perspective_lut)
                .bind_buffer(0, 6, sun)
                .bind_buffer(0, 7, atmos)
                .bind_buffer(0, 8, clouds)
                .bind_buffer(0, 9, camera)
                .set_rasterization({})
                .set_depth_stencil({ .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, blend_info)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, transmittance_lut, aerial_perspective_lut, sun, atmos, clouds, camera);
        });

    const auto rendering_clouds = info.clouds.has_value();
    if (rendering_atmos && rendering_clouds) {
        auto cloud_shape_noise_lut_attachment = self.cloud_shape_noise_lut_view.acquire(
            *self.device, "cloud shape lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
        auto cloud_detail_noise_lut_attachment = self.cloud_detail_noise_lut_view.acquire(
            *self.device, "cloud detail lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

        auto clouds_buffer = transfer_man.scratch_buffer(info.clouds.value());
        std::tie(
            final_attachment,
            transmittance_lut_attachment,
            sky_aerial_perspective_attachment,
            sun_buffer,
            atmos_buffer,
            clouds_buffer,
            camera_buffer) =
            cloud_apply_pass(
                std::move(final_attachment),
                std::move(cloud_shape_noise_lut_attachment),
                std::move(cloud_detail_noise_lut_attachment),
                std::move(transmittance_lut_attachment),
                std::move(sky_aerial_perspective_attachment),
                std::move(sun_buffer),
                std::move(atmos_buffer),
                std::move(clouds_buffer),
                std::move(camera_buffer));
    }

    //  ── VIS TRI ─────────────────────────────────────────────────────────
    for (const auto &mesh : info.rendering_meshes) {
        auto vis_triangle_id_pass = vuk::make_pass(
            "vis triangle",
            [&pipeline = *self.device->pipeline(self.vis_triangle_id_pipeline.id()),
             entities = *self.device->buffer(self.gpu_entities_buffer.id()),
             vertex_buffer = *self.device->buffer(mesh.vertex_buffer_id),
             index_buffer = *self.device->buffer(mesh.index_buffer_id),
             entity_index = mesh.entity_index,
             index_count = mesh.index_count,
             index_offset = mesh.index_offset,
             vertex_offset = mesh.vertex_offset](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eColorWrite) dst,
                VUK_IA(vuk::Access::eDepthStencilRW) dst_depth,
                VUK_BA(vuk::Access::eVertexRead) camera) {
                cmd_list  //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                    .set_depth_stencil(
                        { .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, entities)
                    .bind_buffer(0, 2, vertex_buffer)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                    .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, entity_index)
                    .draw_indexed(index_count, 1, index_offset, static_cast<i32>(vertex_offset), 0);

                return std::make_tuple(dst, dst_depth, camera);
            });

        if (self.gpu_entities_buffer.id() != BufferID::Invalid) {
            std::tie(final_attachment, depth_attachment, camera_buffer) =
                vis_triangle_id_pass(std::move(final_attachment), std::move(depth_attachment), std::move(camera_buffer));
        }
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
