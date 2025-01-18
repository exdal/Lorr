#include "Engine/World/SceneRenderer.hh"

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
        .definitions = { {"LR_BINDLESS_PIPELINE", "1"} },
        .module_name = "vis.triangle_id",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "triangle_id.slang",
        .entry_points = { "vs_main", "fs_main" },
        .bindless_pipeline = true,
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
        [&world_ptr = self.world_ptr, &pipeline = *self.device->pipeline(self.sky_transmittance_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeRW) dst) {
            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec2 image_size = {};
            };

            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, dst)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return dst;
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&world_ptr = self.world_ptr, &pipeline = *self.device->pipeline(self.sky_multiscatter_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            [[maybe_unused]] VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
            VUK_IA(vuk::Access::eComputeRW) dst) {
            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec2 image_size = {};
            };

            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, dst)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst);
        },
        vuk::DomainFlagBits::eGraphicsQueue);
    //   ──────────────────────────────────────────────────────────────────────

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky_transmittance_lut", vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.discard(*self.device, "sky_multiscatter_lut", vuk::ImageUsageFlagBits::eStorage);

    auto temp_scene_info = self.begin_scene({ .has_sun = true, .has_atmosphere = true });
    *temp_scene_info.sun = {};
    *temp_scene_info.atmosphere = {};
    self.end_scene(temp_scene_info);

    transmittance_lut_attachment = transmittance_lut_pass(std::move(transmittance_lut_attachment));
    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment) =
        multiscatter_lut_pass(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));

    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));
}

auto SceneRenderer::begin_scene(this SceneRenderer &self, const SceneRenderBeginInfo &info) -> SceneRenderInfo {
    ZoneScoped;

    // Reset world data
    self.world_data = {};

    auto &transfer_man = self.device->transfer_man();
    SceneRenderInfo result;

    usize world_data_bytes = sizeof(GPUWorldData);
    usize cameras_size_bytes = sizeof(GPUCameraData) * info.camera_count;
    usize model_transforms_bytes = sizeof(GPUModelTransformData) * info.model_transform_count;
    usize buffer_size = world_data_bytes + cameras_size_bytes + model_transforms_bytes;

    buffer_size += info.has_sun ? sizeof(GPUSunData) : 0;
    buffer_size += info.has_atmosphere ? sizeof(GPUAtmosphereData) : 0;

    result.world_data = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, buffer_size);
    result.upload_size += sizeof(GPUWorldData);
    auto ptr = result.world_data.host_ptr();

    if (info.camera_count != 0) {
        result.cameras = reinterpret_cast<GPUCameraData *>(ptr + result.upload_size);

        result.offset_cameras = result.upload_size;
        result.upload_size += cameras_size_bytes;
    }

    if (info.model_transform_count != 0) {
        result.model_transforms = reinterpret_cast<GPUModelTransformData *>(ptr + result.upload_size);

        result.offset_model_transforms = result.upload_size;
        result.upload_size += model_transforms_bytes;
    }

    if (info.has_sun) {
        result.sun = reinterpret_cast<GPUSunData *>(ptr + result.upload_size);

        result.offset_sun = result.upload_size;
        result.upload_size += sizeof(GPUSunData);
    }

    if (info.has_atmosphere) {
        result.atmosphere = reinterpret_cast<GPUAtmosphereData *>(ptr + result.upload_size);

        result.offset_atmosphere = result.upload_size;
        result.upload_size += sizeof(GPUAtmosphereData);
    }

    result.models = std::move(self.rendering_models);

    LS_EXPECT(result.upload_size == buffer_size);

    return result;
}

auto SceneRenderer::end_scene(this SceneRenderer &self, SceneRenderInfo &info) -> void {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();
    auto gpu_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, info.upload_size);

    self.world_data.linear_sampler_index = SlotMap_decode_id(self.linear_sampler.id()).index;
    self.world_data.nearest_sampler_index = SlotMap_decode_id(self.nearest_sampler.id()).index;

    if (info.offset_cameras.has_value()) {
        self.world_data.cameras_ptr = gpu_buffer.device_address() + info.offset_cameras.value();
    }

    if (info.materials_buffer_id != BufferID::Invalid) {
        self.world_data.materials_ptr = self.device->buffer(info.materials_buffer_id)->device_address;
    }

    if (info.offset_model_transforms.has_value()) {
        self.world_data.model_transforms_ptr = gpu_buffer.device_address() + info.offset_model_transforms.value();
    }

    if (info.offset_sun.has_value()) {
        self.world_data.sun_ptr = gpu_buffer.device_address() + info.offset_sun.value();
    }

    if (info.offset_atmosphere.has_value()) {
        self.world_data.atmosphere_ptr = gpu_buffer.device_address() + info.offset_atmosphere.value();
    }

    self.world_data.active_camera_index = info.active_camera_index;
    self.rendering_models = std::move(info.models);

    self.world_ptr = gpu_buffer.device_address();
    std::memcpy(info.world_data.host_ptr<GPUWorldData>(), &self.world_data, sizeof(GPUWorldData));

    transfer_man.upload_staging(info.world_data, gpu_buffer);
}

auto SceneRenderer::render(this SceneRenderer &self, vuk::Format format, vuk::Extent3D extent) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto hdr_format = vuk::Format::eR16G16B16A16Sfloat;
    auto final_attachment =
        vuk::declare_ia("final", { .extent = extent, .format = hdr_format, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia(
        "depth", { .extent = extent, .format = vuk::Format::eD32Sfloat, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::ClearDepth(1.0));

    //  ── SKY VIEW LUT ────────────────────────────────────────────────────
    auto sky_view_pass = vuk::make_pass(
        "sky view",
        [world_ptr = self.world_ptr, &pipeline = *self.device->pipeline(self.sky_view_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut) {
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
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, world_ptr)
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut);
        });

    auto transmittance_lut_attachment = self.sky_transmittance_lut_view.acquire(
        *self.device, "transmittance_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
    auto multiscatter_lut_attachment = self.sky_multiscatter_lut_view.acquire(
        *self.device, "multiscatter_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

    auto sky_view_lut_attachment = vuk::declare_ia(
        "sky_view_lut", { .extent = { .width = 200, .height = 100, .depth = 1 }, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    sky_view_lut_attachment.same_format_as(final_attachment);
    sky_view_lut_attachment = vuk::clear_image(std::move(sky_view_lut_attachment), vuk::Black<f32>);
    std::tie(sky_view_lut_attachment, transmittance_lut_attachment, multiscatter_lut_attachment) =
        sky_view_pass(std::move(sky_view_lut_attachment), std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));

    //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
    auto sky_aerial_perspective_pass = vuk::make_pass(
        "sky aerial perspective",
        [world_ptr = self.world_ptr, &pipeline = *self.device->pipeline(self.sky_aerial_perspective_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut) {
            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec3 image_size = {};
                u32 pad = 0;
            };

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
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, static_cast<u32>(image_size.z));
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut);
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
    std::tie(sky_aerial_perspective_attachment, transmittance_lut_attachment, multiscatter_lut_attachment) = sky_aerial_perspective_pass(
        std::move(sky_aerial_perspective_attachment), std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));

    //  ── SKY FINAL ───────────────────────────────────────────────────────
    auto sky_final_pass = vuk::make_pass(
        "sky final",
        [world_ptr = self.world_ptr, &pipeline = *self.device->pipeline(self.sky_final_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) sky_view_lut,
            VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut) {
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
                .set_rasterization({})
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, world_ptr)
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, sky_view_lut, aerial_perspective_lut, transmittance_lut);
        });

    if (self.world_data.rendering_atmos()) {
        std::tie(final_attachment, sky_view_lut_attachment, sky_aerial_perspective_attachment, transmittance_lut_attachment) =
            sky_final_pass(
                std::move(final_attachment),
                std::move(sky_view_lut_attachment),
                std::move(sky_aerial_perspective_attachment),
                std::move(transmittance_lut_attachment));
    }

    //  ── VIS TRI ─────────────────────────────────────────────────────────
    for (const auto &model : self.rendering_models) {
        auto vis_triangle_id_pass = vuk::make_pass(
            "vis triangle",
            [world_ptr = self.world_ptr,
             &pipeline = *self.device->pipeline(self.vis_triangle_id_pipeline.id()),
             &device = self.device,
             &gpu_model = model](
                vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst, VUK_IA(vuk::Access::eDepthStencilRW) dst_depth) {
                auto vertex_layout = vuk::Packed{
                    vuk::Format::eR32G32B32Sfloat,
                    vuk::Format::eR32G32B32Sfloat,
                    vuk::Format::eR32G32Sfloat,
                    vuk::Format::eR8G8B8A8Unorm,
                };

                struct PushConstants {
                    u64 world_ptr = 0;
                    u32 model_index = 0;
                    u32 pad = 0;
                };

                cmd_list  //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                    .set_depth_stencil(
                        { .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_vertex_buffer(0, *device->buffer(gpu_model.vertex_bufffer_id), 0, vertex_layout)
                    .bind_index_buffer(*device->buffer(gpu_model.index_buffer_id), vuk::IndexType::eUint32);

                for (const auto &mesh : gpu_model.meshes) {
                    for (const auto &meshlet : mesh.meshlets) {
                        cmd_list.push_constants(
                            vuk::ShaderStageFlagBits::eVertex,
                            0,
                            PushConstants{
                                .world_ptr = world_ptr,
                                .model_index = gpu_model.transform_index,
                            });

                        cmd_list.draw_indexed(meshlet.index_count, 1, meshlet.index_offset, static_cast<i32>(meshlet.vertex_offset), 0);
                    }
                }

                return std::make_tuple(dst, dst_depth);
            });

        std::tie(final_attachment, depth_attachment) = vis_triangle_id_pass(std::move(final_attachment), std::move(depth_attachment));
    }

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [world_ptr = self.world_ptr, &pipeline = *self.device->pipeline(self.grid_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst, VUK_IA(vuk::Access::eDepthStencilRW) depth) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, world_ptr)
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth);
        });

    std::tie(final_attachment, depth_attachment) = editor_grid_pass(std::move(final_attachment), std::move(depth_attachment));

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
        vuk::declare_ia("result", { .extent = extent, .format = format, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    std::tie(composition_attachment, final_attachment) = tonemap_pass(std::move(composition_attachment), std::move(final_attachment));
    return composition_attachment;
}

auto SceneRenderer::draw_profiler_ui(this SceneRenderer &) -> void {
    ZoneScoped;
}

}  // namespace lr
