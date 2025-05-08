#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Core/Application.hh"

namespace lr {
enum BindlessDescriptorLayout : u32 {
    Samplers = 0,
    SampledImages = 1,
};

auto SceneRenderer::init(this SceneRenderer &self, Device *device) -> bool {
    self.device = device;

    self.create_persistent_resources();
    return true;
}

auto SceneRenderer::destroy(this SceneRenderer &) -> void {
    ZoneScoped;
}

auto SceneRenderer::create_persistent_resources(this SceneRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &transfer_man = app.device.transfer_man();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);
    auto *materials_set = asset_man.get_materials_descriptor_set();

    //  ── EDITOR ──────────────────────────────────────────────────────────
    auto default_slang_session = self.device->new_slang_session({
        .definitions = {
            { "CULLING_MESHLET_COUNT", std::to_string(Model::MAX_MESHLET_INDICES) },
            { "CULLING_TRIANGLE_COUNT", std::to_string(Model::MAX_MESHLET_PRIMITIVES) },
            { "HISTOGRAM_THREADS_X", std::to_string(GPU::HISTOGRAM_THREADS_X) },
            { "HISTOGRAM_THREADS_Y", std::to_string(GPU::HISTOGRAM_THREADS_Y) },
        },
        .root_directory = shaders_root,
    }).value();
    auto editor_grid_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.editor_grid",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, editor_grid_pipeline_info).value();

    auto editor_mousepick_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.editor_mousepick",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, editor_mousepick_pipeline_info).value();
    //  ── SKY ─────────────────────────────────────────────────────────────
    auto sky_transmittance_lut_info = ImageInfo{
        .format = vuk::Format::eR16G16B16A16Sfloat,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 256, .height = 64, .depth = 1 },
        .name = "Sky Transmittance",
    };
    std::tie(self.sky_transmittance_lut, self.sky_transmittance_lut_view) = Image::create_with_view(*self.device, sky_transmittance_lut_info).value();
    auto sky_transmittance_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_transmittance",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, sky_transmittance_pipeline_info).value();

    auto sky_multiscatter_lut_info = ImageInfo{
        .format = vuk::Format::eR16G16B16A16Sfloat,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 32, .height = 32, .depth = 1 },
        .name = "Sky Multiscatter LUT",
    };
    std::tie(self.sky_multiscatter_lut, self.sky_multiscatter_lut_view) = Image::create_with_view(*self.device, sky_multiscatter_lut_info).value();
    auto sky_multiscatter_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_multiscattering",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, sky_multiscatter_pipeline_info).value();

    auto sky_view_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_view",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, sky_view_pipeline_info).value();

    auto sky_aerial_perspective_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_aerial_perspective",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, sky_aerial_perspective_pipeline_info).value();

    auto sky_final_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_final",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, sky_final_pipeline_info).value();

    //  ── VISBUFFER ───────────────────────────────────────────────────────
    auto vis_cull_meshlets_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.cull_meshlets",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_cull_meshlets_pipeline_info).value();

    auto vis_cull_triangles_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.cull_triangles",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_cull_triangles_pipeline_info).value();

    auto vis_encode_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_encode",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_encode_pipeline_info, *materials_set).value();

    auto vis_clear_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_clear",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_clear_pipeline_info).value();

    auto vis_decode_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_decode",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_decode_pipeline_info, *materials_set).value();

    //  ── PBR ─────────────────────────────────────────────────────────────
    auto pbr_basic_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.brdf",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, pbr_basic_pipeline_info).value();

    //  ── POST PROCESS ────────────────────────────────────────────────────
    auto histogram_generate_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.histogram_generate",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, histogram_generate_pipeline_info).value();

    auto histogram_average_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.histogram_average",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, histogram_average_pipeline_info).value();

    auto tonemap_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.tonemap",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, tonemap_pipeline_info).value();

    auto debug_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.debug",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, debug_pipeline_info).value();

    //  ── SKY LUTS ────────────────────────────────────────────────────────
    auto temp_atmos_info = GPU::Atmosphere{};
    temp_atmos_info.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
    temp_atmos_info.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
    auto temp_atmos = transfer_man.scratch_buffer(temp_atmos_info);

    auto transmittance_lut_pass =
        vuk::make_pass("transmittance lut", [](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::eComputeRW) dst, VUK_BA(vuk::eComputeRead) atmos) {
            cmd_list //
                .bind_compute_pipeline("passes.sky_transmittance")
                .bind_image(0, 0, dst)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(atmos->device_address))
                .dispatch_invocations_per_pixel(dst);

            return std::make_tuple(dst, atmos);
        });

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(transmittance_lut_attachment, temp_atmos) = transmittance_lut_pass(std::move(transmittance_lut_attachment), std::move(temp_atmos));

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [](vuk::CommandBuffer &cmd_list,
           VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
           VUK_IA(vuk::eComputeRW) sky_multiscatter_lut,
           VUK_BA(vuk::eComputeRead) atmos) {
            cmd_list //
                .bind_compute_pipeline("passes.sky_multiscattering")
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, sky_transmittance_lut)
                .bind_image(0, 2, sky_multiscatter_lut)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(atmos->device_address))
                .dispatch_invocations_per_pixel(sky_multiscatter_lut);

            return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, atmos);
        }
    );

    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.discard(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment, temp_atmos) =
        multiscatter_lut_pass(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment), std::move(temp_atmos));

    transmittance_lut_attachment = transmittance_lut_attachment.as_released(vuk::eComputeSampled, vuk::DomainFlagBits::eGraphicsQueue);
    multiscatter_lut_attachment = multiscatter_lut_attachment.as_released(vuk::eComputeSampled, vuk::DomainFlagBits::eGraphicsQueue);
    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));

    self.exposure_buffer = Buffer::create(*self.device, sizeof(GPU::HistogramLuminance)).value();
}

auto SceneRenderer::compose(this SceneRenderer &self, SceneComposeInfo &compose_info) -> ComposedScene {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();

    // IMPORTANT: Only wait when buffer is being resized!!!
    // We can still copy into gpu buffer if it has enough space.

    if (ls::size_bytes(compose_info.gpu_models) > self.models_buffer.data_size()) {
        if (self.models_buffer) {
            self.device->wait();
            self.device->destroy(self.models_buffer.id());
        }

        self.models_buffer = Buffer::create(*self.device, ls::size_bytes(compose_info.gpu_models)).value();
    }

    if (ls::size_bytes(compose_info.gpu_meshlet_instances) > self.meshlet_instances_buffer.data_size()) {
        if (self.meshlet_instances_buffer) {
            self.device->wait();
            self.device->destroy(self.meshlet_instances_buffer.id());
        }

        self.meshlet_instances_buffer = Buffer::create(*self.device, ls::size_bytes(compose_info.gpu_meshlet_instances)).value();
    }

    self.meshlet_instance_count = compose_info.gpu_meshlet_instances.size();
    auto models_buffer = //
        transfer_man.upload_staging(ls::span(compose_info.gpu_models), self.models_buffer);
    auto meshlet_instances_buffer = //
        transfer_man.upload_staging(ls::span(compose_info.gpu_meshlet_instances), self.meshlet_instances_buffer);

    return ComposedScene{
        .models_buffer = models_buffer,
        .meshlet_instances_buffer = meshlet_instances_buffer,
    };
}

auto SceneRenderer::cleanup(this SceneRenderer &self) -> void {
    ZoneScoped;

    auto &device = *self.device;

    device.wait();

    vuk::fill(vuk::acquire_buf("exposure", *device.buffer(self.exposure_buffer.id()), vuk::eNone), 0);

    self.meshlet_instance_count = 0;

    if (self.transforms_buffer) {
        device.destroy(self.transforms_buffer.id());
        self.transforms_buffer = {};
    }

    if (self.meshlet_instances_buffer) {
        device.destroy(self.meshlet_instances_buffer.id());
        self.meshlet_instances_buffer = {};
    }

    if (self.models_buffer) {
        device.destroy(self.models_buffer.id());
        self.models_buffer = {};
    }
}

auto SceneRenderer::render(this SceneRenderer &self, SceneRenderInfo &info, ls::option<ComposedScene> &composed_scene)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();

    //  ── ENTITY TRANSFORMS ───────────────────────────────────────────────
    //
    // WARN: compose_info.transforms contains _ALL_ transforms!!!
    //
    bool rebuild_transforms = false;
    if (info.transforms.size_bytes() > self.transforms_buffer.data_size()) {
        if (self.transforms_buffer.id() != BufferID::Invalid) {
            // Device wait here is important, do not remove it. Why?
            // We are using ONE transform buffer for all frames, if
            // this buffer gets destroyed in current frame, previous
            // rendering frame buffer will get corrupt and crash GPU.
            self.device->wait();
            self.device->destroy(self.transforms_buffer.id());
        }

        self.transforms_buffer = Buffer::create(*self.device, info.transforms.size_bytes(), vuk::MemoryUsage::eGPUonly).value();

        rebuild_transforms = true;
    }

    auto transforms_buffer = self.transforms_buffer.acquire(*self.device, "Transforms Buffer", vuk::Access::eMemoryRead);

    if (rebuild_transforms) {
        transforms_buffer = transfer_man.upload_staging(info.transforms, std::move(transforms_buffer));
    } else if (!info.dirty_transform_ids.empty()) {
        auto transform_count = info.dirty_transform_ids.size();
        auto new_transforms_size_bytes = transform_count * sizeof(GPU::Transforms);
        auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, new_transforms_size_bytes);
        auto *dst_transform_ptr = reinterpret_cast<GPU::Transforms *>(upload_buffer->mapped_ptr);
        auto upload_offsets = std::vector<u64>(transform_count);

        for (const auto &[dirty_transform_id, offset] : std::views::zip(info.dirty_transform_ids, upload_offsets)) {
            auto index = SlotMap_decode_id(dirty_transform_id).index;
            const auto &transform = info.transforms[index];
            std::memcpy(dst_transform_ptr, &transform, sizeof(GPU::Transforms));
            offset = index * sizeof(GPU::Transforms);
            dst_transform_ptr++;
        }

        auto update_transforms_pass = vuk::make_pass(
            "update scene transforms",
            [upload_offsets = std::move(
                 upload_offsets
             )]( //
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::Access::eTransferRead) src_buffer,
                VUK_BA(vuk::Access::eTransferWrite) dst_buffer
            ) {
                for (usize i = 0; i < upload_offsets.size(); i++) {
                    auto offset = upload_offsets[i];
                    auto src_subrange = src_buffer->subrange(i * sizeof(GPU::Transforms), sizeof(GPU::Transforms));
                    auto dst_subrange = dst_buffer->subrange(offset, sizeof(GPU::Transforms));
                    cmd_list.copy_buffer(src_subrange, dst_subrange);
                }

                return dst_buffer;
            }
        );

        transforms_buffer = update_transforms_pass(std::move(upload_buffer), std::move(transforms_buffer));
    }

    //   ──────────────────────────────────────────────────────────────────────
    auto final_attachment = vuk::declare_ia(
        "final",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .extent = info.extent,
          .format = vuk::Format::eB10G11R11UfloatPack32,
          .sample_count = vuk::Samples::e1,
          .level_count = 1,
          .layer_count = 1 }
    );
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto result_attachment = vuk::declare_ia(
        "result",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = info.format,
          .sample_count = vuk::Samples::e1 }
    );
    result_attachment.same_shape_as(final_attachment);
    result_attachment = vuk::clear_image(std::move(result_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia(
        "depth",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eDepthStencilAttachment,
          .format = vuk::Format::eD32Sfloat,
          .sample_count = vuk::Samples::e1 }
    );
    depth_attachment.same_shape_as(final_attachment);
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::ClearDepth(1.0));

    auto sky_transmittance_lut_attachment =
        self.sky_transmittance_lut_view
            .acquire(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
    auto sky_multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.acquire(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);

    //   ──────────────────────────────────────────────────────────────────────

    const auto debugging = self.debug_view != GPU::DebugView::None;
    auto sun_buffer = vuk::Value<vuk::Buffer>{};
    if (info.sun.has_value()) {
        sun_buffer = transfer_man.scratch_buffer(info.sun.value());
    }

    auto atmosphere_buffer = vuk::Value<vuk::Buffer>{};
    if (info.atmosphere.has_value()) {
        auto atmosphere = info.atmosphere.value();
        atmosphere.sky_view_lut_size = self.sky_view_lut_extent;
        atmosphere.aerial_perspective_lut_size = self.sky_aerial_perspective_lut_extent;
        atmosphere.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
        atmosphere.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
        atmosphere_buffer = transfer_man.scratch_buffer(atmosphere);
    }

    auto camera_buffer = vuk::Value<vuk::Buffer>{};
    if (info.camera.has_value()) {
        camera_buffer = transfer_man.scratch_buffer(info.camera.value());
    }

    if (self.meshlet_instance_count) {
        vuk::Value<vuk::Buffer> models_buffer;
        vuk::Value<vuk::Buffer> meshlet_instances_buffer;
        if (composed_scene.has_value()) {
            models_buffer = std::move(composed_scene->models_buffer);
            meshlet_instances_buffer = std::move(composed_scene->meshlet_instances_buffer);
        } else {
            models_buffer = self.models_buffer.acquire(*self.device, "Models", vuk::Access::eNone);
            meshlet_instances_buffer = self.meshlet_instances_buffer.acquire(*self.device, "Meshlet Instances", vuk::Access::eNone);
        }

        auto materials_buffer = std::move(info.materials_buffer);
        auto *materials_set = info.materials_descriptor_set;

        //  ── CULL MESHLETS ───────────────────────────────────────────────────
        auto vis_cull_meshlets_pass = vuk::make_pass(
            "vis cull meshlets",
            [meshlet_instance_count = self.meshlet_instance_count, cull_flags = info.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeWrite) cull_triangles_cmd,
                VUK_BA(vuk::eComputeWrite) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRead) models,
                VUK_BA(vuk::eComputeRead) camera
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.cull_meshlets")
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants(
                            cull_triangles_cmd->device_address,
                            visible_meshlet_instances_indices->device_address,
                            meshlet_instances->device_address,
                            transforms->device_address,
                            models->device_address,
                            camera->device_address,
                            meshlet_instance_count,
                            cull_flags
                        )
                    )
                    .dispatch((meshlet_instance_count + Model::MAX_MESHLET_INDICES - 1) / Model::MAX_MESHLET_INDICES);
                return std::make_tuple(cull_triangles_cmd, visible_meshlet_instances_indices, meshlet_instances, transforms, models, camera);
            }
        );

        auto cull_triangles_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
        auto visible_meshlet_instances_indices_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * sizeof(u32));

        std::tie(
            cull_triangles_cmd_buffer,
            visible_meshlet_instances_indices_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            models_buffer,
            camera_buffer
        ) =
            vis_cull_meshlets_pass(
                std::move(cull_triangles_cmd_buffer),
                std::move(visible_meshlet_instances_indices_buffer),
                std::move(meshlet_instances_buffer),
                std::move(transforms_buffer),
                std::move(models_buffer),
                std::move(camera_buffer)
            );

        //  ── CULL TRIANGLES ──────────────────────────────────────────────────
        auto vis_cull_triangles_pass = vuk::make_pass(
            "vis cull triangles",
            [cull_flags = info.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eIndirectRead) cull_triangles_cmd,
                VUK_BA(vuk::eComputeWrite) draw_indexed_cmd,
                VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeWrite) reordered_indices,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRead) models,
                VUK_BA(vuk::eComputeWrite) camera
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.cull_triangles")
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants(
                            draw_indexed_cmd->device_address,
                            visible_meshlet_instances_indices->device_address,
                            reordered_indices->device_address,
                            meshlet_instances->device_address,
                            transforms->device_address,
                            models->device_address,
                            camera->device_address,
                            cull_flags
                        )
                    )
                    .dispatch_indirect(cull_triangles_cmd);
                return std::make_tuple(
                    draw_indexed_cmd,
                    visible_meshlet_instances_indices,
                    reordered_indices,
                    meshlet_instances,
                    transforms,
                    models,
                    camera
                );
            }
        );

        auto draw_command_buffer = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });
        auto reordered_indices_buffer = transfer_man.alloc_transient_buffer(
            vuk::MemoryUsage::eGPUonly,
            self.meshlet_instance_count * Model::MAX_MESHLET_PRIMITIVES * 3 * sizeof(u32)
        );

        std::tie(
            draw_command_buffer,
            visible_meshlet_instances_indices_buffer,
            reordered_indices_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            models_buffer,
            camera_buffer
        ) =
            vis_cull_triangles_pass(
                std::move(cull_triangles_cmd_buffer),
                std::move(draw_command_buffer),
                std::move(visible_meshlet_instances_indices_buffer),
                std::move(reordered_indices_buffer),
                std::move(meshlet_instances_buffer),
                std::move(transforms_buffer),
                std::move(models_buffer),
                std::move(camera_buffer)
            );

        //  ── VISBUFFER CLEAR ─────────────────────────────────────────────────
        auto vis_clear_pass = vuk::make_pass(
            "vis clear",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eComputeWrite) visbuffer,
                VUK_IA(vuk::eComputeWrite) visbuffer_data,
                VUK_IA(vuk::eComputeWrite) overdraw
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.visbuffer_clear")
                    .bind_image(0, 0, visbuffer)
                    .bind_image(0, 1, visbuffer_data)
                    .bind_image(0, 2, overdraw)
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants(glm::uvec2(visbuffer->extent.width, visbuffer->extent.height))
                    )
                    .dispatch_invocations_per_pixel(visbuffer);

                return std::make_tuple(visbuffer, visbuffer_data, overdraw);
            }
        );

        auto visbuffer_attachment = vuk::declare_ia(
            "visbuffer",
            { .usage = vuk::ImageUsageFlagBits::eStorage, //
              .format = vuk::Format::eR64Uint,
              .sample_count = vuk::Samples::e1 }
        );
        visbuffer_attachment.same_shape_as(final_attachment);

        auto visbuffer_data_attachment = vuk::declare_ia(
            "visbuffer data",
            { .usage = vuk::ImageUsageFlagBits::eStorage | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR32Uint,
              .sample_count = vuk::Samples::e1 }
        );
        visbuffer_data_attachment.same_shape_as(final_attachment);

        auto overdraw_attachment = vuk::declare_ia(
            "overdraw",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR32Uint,
              .sample_count = vuk::Samples::e1 }
        );
        overdraw_attachment.same_shape_as(final_attachment);

        std::tie(visbuffer_attachment, visbuffer_data_attachment, overdraw_attachment) =
            vis_clear_pass(std::move(visbuffer_attachment), std::move(visbuffer_data_attachment), std::move(overdraw_attachment));

        //  ── VISBUFFER ENCODE ────────────────────────────────────────────────
        auto vis_encode_pass = vuk::make_pass(
            "vis encode",
            [descriptor_set = materials_set](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eIndirectRead) triangle_indirect,
                VUK_BA(vuk::eIndexRead) index_buffer,
                VUK_IA(vuk::eColorWrite) visbuffer,
                VUK_IA(vuk::eDepthStencilRW) depth,
                VUK_BA(vuk::eVertexRead) camera,
                VUK_BA(vuk::eVertexRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eVertexRead) meshlet_instances,
                VUK_BA(vuk::eVertexRead) transforms,
                VUK_BA(vuk::eVertexRead) models,
                VUK_BA(vuk::eFragmentRead) materials,
                VUK_IA(vuk::eFragmentRW) overdraw
            ) {
                cmd_list //
                    .bind_graphics_pipeline("passes.visbuffer_encode")
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                    .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(visbuffer, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_persistent(1, *descriptor_set)
                    .bind_image(0, 0, overdraw)
                    .bind_buffer(0, 1, camera)
                    .bind_buffer(0, 2, visible_meshlet_instances_indices)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, models)
                    .bind_buffer(0, 5, transforms)
                    .bind_buffer(0, 6, materials)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                    .draw_indexed_indirect(1, triangle_indirect);
                return std::make_tuple(
                    visbuffer,
                    depth,
                    camera,
                    visible_meshlet_instances_indices,
                    meshlet_instances,
                    transforms,
                    models,
                    materials,
                    overdraw
                );
            }
        );

        std::tie(
            visbuffer_data_attachment,
            depth_attachment,
            camera_buffer,
            visible_meshlet_instances_indices_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            models_buffer,
            materials_buffer,
            overdraw_attachment
        ) =
            vis_encode_pass(
                std::move(draw_command_buffer),
                std::move(reordered_indices_buffer),
                std::move(visbuffer_data_attachment),
                std::move(depth_attachment),
                std::move(camera_buffer),
                std::move(visible_meshlet_instances_indices_buffer),
                std::move(meshlet_instances_buffer),
                std::move(transforms_buffer),
                std::move(models_buffer),
                std::move(materials_buffer),
                std::move(overdraw_attachment)
            );

        //  ── EDITOR MOUSE PICKING ────────────────────────────────────────────
        if (info.picking_texel) {
            auto editor_mousepick_pass = vuk::make_pass(
                "editor mousepick",
                [picking_texel = info.picking_texel.value(
                 )]( //
                    vuk::CommandBuffer &cmd_list,
                    VUK_IA(vuk::eComputeRead) visbuffer,
                    VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
                    VUK_BA(vuk::eComputeRead) meshlet_instances,
                    VUK_BA(vuk::eComputeWrite) picked_transform_index_buffer
                ) {
                    cmd_list //
                        .bind_compute_pipeline("passes.editor_mousepick")
                        .bind_image(0, 0, visbuffer)
                        .bind_buffer(0, 1, visible_meshlet_instances_indices)
                        .bind_buffer(0, 2, meshlet_instances)
                        .push_constants(
                            vuk::ShaderStageFlagBits::eCompute,
                            0,
                            PushConstants(picked_transform_index_buffer->device_address, picking_texel)
                        )
                        .dispatch(1);

                    return picked_transform_index_buffer;
                }
            );

            auto picking_texel_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, sizeof(u32));
            auto picked_texel = editor_mousepick_pass(
                visbuffer_data_attachment,
                visible_meshlet_instances_indices_buffer,
                meshlet_instances_buffer,
                picking_texel_buffer
            );

            vuk::Compiler temp_compiler;
            picked_texel.wait(self.device->get_allocator(), temp_compiler);

            u32 texel_data = 0;
            std::memcpy(&texel_data, picked_texel->mapped_ptr, sizeof(u32));
            info.picked_transform_index = texel_data;
        }

        //  ── VISBUFFER DECODE ────────────────────────────────────────────────
        auto vis_decode_pass = vuk::make_pass(
            "vis decode",
            [descriptor_set = materials_set]( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) albedo,
                VUK_IA(vuk::eColorWrite) normal,
                VUK_IA(vuk::eColorWrite) emissive,
                VUK_IA(vuk::eColorWrite) metallic_roughness_occlusion,
                VUK_BA(vuk::eFragmentRead) camera,
                VUK_BA(vuk::eFragmentRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eFragmentRead) meshlet_instances,
                VUK_BA(vuk::eFragmentRead) models,
                VUK_BA(vuk::eFragmentRead) transforms,
                VUK_BA(vuk::eFragmentRead) materials,
                VUK_IA(vuk::eFragmentRead) visbuffer
            ) {
                cmd_list //
                    .bind_graphics_pipeline("passes.visbuffer_decode")
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                    .set_depth_stencil({})
                    .set_color_blend(albedo, vuk::BlendPreset::eOff)
                    .set_color_blend(normal, vuk::BlendPreset::eOff)
                    .set_color_blend(emissive, vuk::BlendPreset::eOff)
                    .set_color_blend(metallic_roughness_occlusion, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_image(0, 0, visbuffer)
                    .bind_buffer(0, 1, camera)
                    .bind_buffer(0, 2, visible_meshlet_instances_indices)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, models)
                    .bind_buffer(0, 5, transforms)
                    .bind_buffer(0, 6, materials)
                    .bind_persistent(1, *descriptor_set)
                    .draw(3, 1, 0, 1);

                return std::make_tuple(
                    albedo,
                    normal,
                    emissive,
                    metallic_roughness_occlusion,
                    camera,
                    visible_meshlet_instances_indices,
                    meshlet_instances,
                    transforms,
                    visbuffer
                );
            }
        );

        auto albedo_attachment = vuk::declare_ia(
            "albedo",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR8G8B8A8Srgb,
              .sample_count = vuk::Samples::e1 }
        );
        albedo_attachment.same_shape_as(visbuffer_data_attachment);
        albedo_attachment = vuk::clear_image(std::move(albedo_attachment), vuk::Black<f32>);

        auto normal_attachment = vuk::declare_ia(
            "normal",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR16G16B16A16Sfloat,
              .sample_count = vuk::Samples::e1 }
        );
        normal_attachment.same_shape_as(visbuffer_data_attachment);
        normal_attachment = vuk::clear_image(std::move(normal_attachment), vuk::Black<f32>);

        auto emissive_attachment = vuk::declare_ia(
            "emissive",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eB10G11R11UfloatPack32,
              .sample_count = vuk::Samples::e1 }
        );
        emissive_attachment.same_shape_as(visbuffer_data_attachment);
        emissive_attachment = vuk::clear_image(std::move(emissive_attachment), vuk::Black<f32>);

        auto metallic_roughness_occlusion_attachment = vuk::declare_ia(
            "metallic roughness occlusion",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR8G8B8A8Unorm,
              .sample_count = vuk::Samples::e1 }
        );
        metallic_roughness_occlusion_attachment.same_shape_as(visbuffer_data_attachment);
        metallic_roughness_occlusion_attachment = vuk::clear_image(std::move(metallic_roughness_occlusion_attachment), vuk::Black<f32>);

        std::tie(
            albedo_attachment,
            normal_attachment,
            emissive_attachment,
            metallic_roughness_occlusion_attachment,
            camera_buffer,
            visible_meshlet_instances_indices_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            visbuffer_data_attachment
        ) =
            vis_decode_pass(
                std::move(albedo_attachment),
                std::move(normal_attachment),
                std::move(emissive_attachment),
                std::move(metallic_roughness_occlusion_attachment),
                std::move(camera_buffer),
                std::move(visible_meshlet_instances_indices_buffer),
                std::move(meshlet_instances_buffer),
                std::move(models_buffer),
                std::move(transforms_buffer),
                std::move(materials_buffer),
                std::move(visbuffer_data_attachment)
            );

        if (!debugging && info.atmosphere.has_value()) {
            //  ── BRDF ────────────────────────────────────────────────────────────
            auto brdf_pass = vuk::make_pass(
                "brdf",
                []( //
                    vuk::CommandBuffer &cmd_list,
                    VUK_IA(vuk::eColorWrite) dst,
                    VUK_BA(vuk::eFragmentRead) atmosphere,
                    VUK_BA(vuk::eFragmentRead) sun,
                    VUK_BA(vuk::eFragmentRead) camera,
                    VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
                    VUK_IA(vuk::eFragmentSampled) sky_multiscatter_lut,
                    VUK_IA(vuk::eFragmentRead) depth,
                    VUK_IA(vuk::eFragmentSampled) albedo,
                    VUK_IA(vuk::eFragmentRead) normal,
                    VUK_IA(vuk::eFragmentRead) emissive,
                    VUK_IA(vuk::eFragmentRead) metallic_roughness_occlusion
                ) {
                    auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eLinear,
                        .minFilter = vuk::Filter::eLinear,
                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                    };

                    auto linear_repeat_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eLinear,
                        .minFilter = vuk::Filter::eLinear,
                        .addressModeU = vuk::SamplerAddressMode::eRepeat,
                        .addressModeV = vuk::SamplerAddressMode::eRepeat,
                    };

                    cmd_list //
                        .bind_graphics_pipeline("passes.brdf")
                        .set_rasterization({})
                        .set_color_blend(dst, vuk::BlendPreset::eOff)
                        .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                        .set_viewport(0, vuk::Rect2D::framebuffer())
                        .set_scissor(0, vuk::Rect2D::framebuffer())
                        .bind_sampler(0, 0, linear_clamp_sampler)
                        .bind_sampler(0, 1, linear_repeat_sampler)
                        .bind_image(0, 2, sky_transmittance_lut)
                        .bind_image(0, 3, sky_multiscatter_lut)
                        .bind_image(0, 4, depth)
                        .bind_image(0, 5, albedo)
                        .bind_image(0, 6, normal)
                        .bind_image(0, 7, emissive)
                        .bind_image(0, 8, metallic_roughness_occlusion)
                        .push_constants(
                            vuk::ShaderStageFlagBits::eFragment,
                            0,
                            PushConstants(atmosphere->device_address, sun->device_address, camera->device_address)
                        )
                        .draw(3, 1, 0, 0);
                    return std::make_tuple(dst, atmosphere, sun, camera, sky_transmittance_lut, sky_multiscatter_lut, depth);
                }
            );

            std::tie(
                final_attachment,
                atmosphere_buffer,
                sun_buffer,
                camera_buffer,
                sky_transmittance_lut_attachment,
                sky_multiscatter_lut_attachment,
                depth_attachment
            ) =
                brdf_pass(
                    std::move(final_attachment),
                    std::move(atmosphere_buffer),
                    std::move(sun_buffer),
                    std::move(camera_buffer),
                    std::move(sky_transmittance_lut_attachment),
                    std::move(sky_multiscatter_lut_attachment),
                    std::move(depth_attachment),
                    std::move(albedo_attachment),
                    std::move(normal_attachment),
                    std::move(emissive_attachment),
                    std::move(metallic_roughness_occlusion_attachment)
                );
        } else {
            auto debug_pass = vuk::make_pass(
                "debug pass",
                [debug_view = self.debug_view,
                 heatmap_scale = self.debug_heatmap_scale]( //
                    vuk::CommandBuffer &cmd_list,
                    VUK_IA(vuk::eColorWrite) dst,
                    VUK_IA(vuk::eFragmentRead) visbuffer,
                    VUK_IA(vuk::eFragmentSampled) depth,
                    VUK_IA(vuk::eFragmentRead) overdraw,
                    VUK_IA(vuk::eFragmentSampled) albedo,
                    VUK_IA(vuk::eFragmentRead) normal,
                    VUK_IA(vuk::eFragmentRead) emissive,
                    VUK_IA(vuk::eFragmentRead) metallic_roughness_occlusion,
                    VUK_BA(vuk::eFragmentRead) camera,
                    VUK_BA(vuk::eFragmentRead) visible_meshlet_instances_indices
                ) {
                    auto linear_repeat_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eLinear,
                        .minFilter = vuk::Filter::eLinear,
                        .addressModeU = vuk::SamplerAddressMode::eRepeat,
                        .addressModeV = vuk::SamplerAddressMode::eRepeat,
                    };

                    cmd_list //
                        .bind_graphics_pipeline("passes.debug")
                        .set_rasterization({})
                        .set_color_blend(dst, vuk::BlendPreset::eOff)
                        .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                        .set_viewport(0, vuk::Rect2D::framebuffer())
                        .set_scissor(0, vuk::Rect2D::framebuffer())
                        .bind_sampler(0, 0, linear_repeat_sampler)
                        .bind_image(0, 1, visbuffer)
                        .bind_image(0, 2, depth)
                        .bind_image(0, 3, overdraw)
                        .bind_image(0, 4, albedo)
                        .bind_image(0, 5, normal)
                        .bind_image(0, 6, emissive)
                        .bind_image(0, 7, metallic_roughness_occlusion)
                        .bind_buffer(0, 8, camera)
                        .bind_buffer(0, 9, visible_meshlet_instances_indices)
                        .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, PushConstants(std::to_underlying(debug_view), heatmap_scale))
                        .draw(3, 1, 0, 0);

                    return dst;
                }
            );

            result_attachment = debug_pass(
                std::move(result_attachment),
                std::move(visbuffer_data_attachment),
                std::move(depth_attachment),
                std::move(overdraw_attachment),
                std::move(albedo_attachment),
                std::move(normal_attachment),
                std::move(emissive_attachment),
                std::move(metallic_roughness_occlusion_attachment),
                std::move(camera_buffer),
                std::move(visible_meshlet_instances_indices_buffer)
            );
        }
    }

    if (info.atmosphere.has_value() && !debugging) {
        auto sky_view_lut_attachment = vuk::declare_ia(
            "sky view lut",
            { .image_type = vuk::ImageType::e2D,
              .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
              .extent = self.sky_view_lut_extent,
              .format = vuk::Format::eR16G16B16A16Sfloat,
              .sample_count = vuk::Samples::e1,
              .view_type = vuk::ImageViewType::e2D,
              .level_count = 1,
              .layer_count = 1 }
        );

        auto sky_aerial_perspective_attachment = vuk::declare_ia(
            "sky aerial perspective",
            { .image_type = vuk::ImageType::e3D,
              .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
              .extent = self.sky_aerial_perspective_lut_extent,
              .sample_count = vuk::Samples::e1,
              .view_type = vuk::ImageViewType::e3D,
              .level_count = 1,
              .layer_count = 1 }
        );
        sky_aerial_perspective_attachment.same_format_as(sky_view_lut_attachment);

        //  ── SKY VIEW LUT ────────────────────────────────────────────────────
        auto sky_view_pass = vuk::make_pass(
            "sky view",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) atmosphere,
                VUK_BA(vuk::eComputeRead) sun,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                VUK_IA(vuk::eComputeRW) sky_view_lut
            ) {
                auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                };

                cmd_list //
                    .bind_compute_pipeline("passes.sky_view")
                    .bind_sampler(0, 0, linear_clamp_sampler)
                    .bind_image(0, 1, sky_transmittance_lut)
                    .bind_image(0, 2, sky_multiscatter_lut)
                    .bind_image(0, 3, sky_view_lut)
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants(atmosphere->device_address, sun->device_address, camera->device_address)
                    )
                    .dispatch_invocations_per_pixel(sky_view_lut);
                return std::make_tuple(sky_view_lut, sky_transmittance_lut, sky_multiscatter_lut, atmosphere, sun, camera);
            }
        );
        std::tie(
            sky_view_lut_attachment,
            sky_transmittance_lut_attachment,
            sky_multiscatter_lut_attachment,
            atmosphere_buffer,
            sun_buffer,
            camera_buffer
        ) =
            sky_view_pass(
                std::move(atmosphere_buffer),
                std::move(sun_buffer),
                std::move(camera_buffer),
                std::move(sky_transmittance_lut_attachment),
                std::move(sky_multiscatter_lut_attachment),
                std::move(sky_view_lut_attachment)
            );

        //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
        auto sky_aerial_perspective_pass = vuk::make_pass(
            "sky aerial perspective",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) atmosphere,
                VUK_BA(vuk::eComputeRead) sun,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                VUK_IA(vuk::eComputeRW) sky_aerial_perspective_lut
            ) {
                auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                };

                cmd_list //
                    .bind_compute_pipeline("passes.sky_aerial_perspective")
                    .bind_sampler(0, 0, linear_clamp_sampler)
                    .bind_image(0, 1, sky_transmittance_lut)
                    .bind_image(0, 2, sky_multiscatter_lut)
                    .bind_image(0, 3, sky_aerial_perspective_lut)
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants(atmosphere->device_address, sun->device_address, camera->device_address)
                    )
                    .dispatch_invocations_per_pixel(sky_aerial_perspective_lut);
                return std::make_tuple(sky_aerial_perspective_lut, sky_transmittance_lut, atmosphere, sun, camera);
            }
        );

        std::tie(sky_aerial_perspective_attachment, sky_transmittance_lut_attachment, atmosphere_buffer, sun_buffer, camera_buffer) =
            sky_aerial_perspective_pass(
                std::move(atmosphere_buffer),
                std::move(sun_buffer),
                std::move(camera_buffer),
                std::move(sky_transmittance_lut_attachment),
                std::move(sky_multiscatter_lut_attachment),
                std::move(sky_aerial_perspective_attachment)
            );

        //  ── SKY FINAL ───────────────────────────────────────────────────────
        auto sky_final_pass = vuk::make_pass(
            "sky final",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) dst,
                VUK_BA(vuk::eFragmentRead) atmosphere,
                VUK_BA(vuk::eFragmentRead) sun,
                VUK_BA(vuk::eFragmentRead) camera,
                VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
                VUK_IA(vuk::eFragmentSampled) sky_aerial_perspective_lut,
                VUK_IA(vuk::eFragmentSampled) sky_view_lut,
                VUK_IA(vuk::eFragmentSampled) depth
            ) {
                auto linear_clamp_sampler = vuk::SamplerCreateInfo{
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

                cmd_list //
                    .bind_graphics_pipeline("passes.sky_final")
                    .set_rasterization({})
                    .set_depth_stencil({})
                    .set_color_blend(dst, blend_info)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_sampler(0, 0, linear_clamp_sampler)
                    .bind_image(0, 1, sky_transmittance_lut)
                    .bind_image(0, 2, sky_aerial_perspective_lut)
                    .bind_image(0, 3, sky_view_lut)
                    .bind_image(0, 4, depth)
                    .push_constants(
                        vuk::ShaderStageFlagBits::eFragment,
                        0,
                        PushConstants(atmosphere->device_address, sun->device_address, camera->device_address)
                    )
                    .draw(3, 1, 0, 0);
                return std::make_tuple(dst, depth, camera);
            }
        );

        std::tie(final_attachment, depth_attachment, camera_buffer) = sky_final_pass(
            std::move(final_attachment),
            std::move(atmosphere_buffer),
            std::move(sun_buffer),
            std::move(camera_buffer),
            std::move(sky_transmittance_lut_attachment),
            std::move(sky_aerial_perspective_attachment),
            std::move(sky_view_lut_attachment),
            std::move(depth_attachment)
        );
    }

    if (!debugging) {
        auto histogram_info = info.histogram_info.value_or(GPU::HistogramInfo{});

        //  ── HISTOGRAM GENERATE ──────────────────────────────────────────────
        auto histogram_generate_pass = vuk::make_pass(
            "histogram generate",
            [histogram_info]( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eComputeRead) src,
                VUK_BA(vuk::eComputeRW) histogram
            ) {
                cmd_list.bind_compute_pipeline("passes.histogram_generate")
                    .bind_image(0, 0, src)
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants( //
                            histogram->device_address,
                            glm::uvec2(src->extent.width, src->extent.height),
                            histogram_info.min_exposure,
                            1.0f / (histogram_info.max_exposure - histogram_info.min_exposure)
                        )
                    )
                    .dispatch_invocations_per_pixel(src);

                return std::make_tuple(src, histogram);
            }
        );

        auto histogram_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, GPU::HISTOGRAM_BIN_COUNT * sizeof(u32));
        vuk::fill(histogram_buffer, 0);
        std::tie(final_attachment, histogram_buffer) = histogram_generate_pass(std::move(final_attachment), std::move(histogram_buffer));

        //  ── HISTOGRAM AVERAGE ───────────────────────────────────────────────
        auto histogram_average_pass = vuk::make_pass(
            "histogram average",
            [pixel_count = f32(final_attachment->extent.width * final_attachment->extent.height), delta_time = info.delta_time, histogram_info]( //
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRW) histogram,
                VUK_BA(vuk::eComputeRW) exposure
            ) {
                cmd_list.bind_compute_pipeline("passes.histogram_average")
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants( //
                            histogram->device_address,
                            exposure->device_address,
                            pixel_count,
                            histogram_info.min_exposure,
                            histogram_info.max_exposure - histogram_info.min_exposure,
                            glm::clamp(static_cast<f32>(1.0f - glm::exp(-histogram_info.adaptation_speed * delta_time)), 0.0f, 1.0f),
                            histogram_info.ev100_bias
                        )
                    )
                    .dispatch(1);

                return exposure;
            }
        );

        auto exposure_buffer = self.exposure_buffer.acquire(*self.device, "exposure", vuk::eNone);
        if (info.histogram_info.has_value()) {
            exposure_buffer = histogram_average_pass(std::move(histogram_buffer), std::move(exposure_buffer));
        }

        //  ── TONEMAP ─────────────────────────────────────────────────────────
        auto tonemap_pass = vuk::make_pass(
            "tonemap",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) dst,
                VUK_IA(vuk::eFragmentSampled) src,
                VUK_BA(vuk::eFragmentRead) exposure
            ) {
                cmd_list //
                    .bind_graphics_pipeline("passes.tonemap")
                    .set_rasterization({})
                    .set_color_blend(dst, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                    .bind_image(0, 1, src)
                    .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, exposure->device_address)
                    .draw(3, 1, 0, 0);
                return dst;
            }
        );
        result_attachment = tonemap_pass(std::move(result_attachment), std::move(final_attachment), std::move(exposure_buffer));
    }

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        []( //
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::eColorWrite) dst,
            VUK_IA(vuk::eDepthStencilWrite) depth,
            VUK_BA(vuk::eVertexRead | vuk::eFragmentRead) camera
        ) {
            cmd_list //
                .bind_graphics_pipeline("passes.editor_grid")
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, PushConstants(camera->device_address))
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth);
        }
    );
    // std::tie(result_attachment, depth_attachment) =
    //     editor_grid_pass(std::move(result_attachment), std::move(depth_attachment), std::move(camera_buffer));

    return result_attachment;
}

} // namespace lr
