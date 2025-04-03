#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Core/Application.hh"

namespace lr {
auto SceneRenderer::init(this SceneRenderer &self, Device *device) -> bool {
    self.device = device;

    self.setup_persistent_resources();
    return true;
}

auto SceneRenderer::destroy(this SceneRenderer &) -> void {
    ZoneScoped;
}

auto SceneRenderer::setup_persistent_resources(this SceneRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &transfer_man = app.device.transfer_man();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    // clang-format off
    self.linear_repeat_sampler = Sampler::create(
        *self.device,
        vuk::Filter::eLinear,
        vuk::Filter::eLinear,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::CompareOp::eNever).value();
    self.linear_clamp_sampler = Sampler::create(
        *self.device,
        vuk::Filter::eLinear,
        vuk::Filter::eLinear,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::CompareOp::eNever).value();
    self.nearest_repeat_sampler = Sampler::create(
        *self.device,
        vuk::Filter::eNearest,
        vuk::Filter::eNearest,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::CompareOp::eNever).value();
    //  ── GRID ────────────────────────────────────────────────────────────
    self.grid_pipeline = Pipeline::create(*self.device, {
        .module_name = "editor.grid",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "editor_grid.slang",
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
        .module_name = "sky_transmittance",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_transmittance.slang",
        .entry_points = { "cs_main" },
    }, self.device->bindless_set()).value();
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
        .module_name = "sky_multiscattering",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_multiscattering.slang",
        .entry_points = { "cs_main" },
    }, self.device->bindless_set()).value();
    self.sky_view_pipeline = Pipeline::create(*self.device, {
        .module_name = "sky_view",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_view.slang",
        .entry_points = { "cs_main" },
    }, self.device->bindless_set()).value();
    self.sky_aerial_perspective_pipeline = Pipeline::create(*self.device, {
        .module_name = "sky_aerial_perspective",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_aerial_perspective.slang",
        .entry_points = { "cs_main" },
    }, self.device->bindless_set()).value();
    self.sky_final_pipeline = Pipeline::create(*self.device, {
        .module_name = "sky_view",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_final.slang",
        .entry_points = { "vs_main", "fs_main" },
    }, self.device->bindless_set()).value();
    //  ── VISBUFFER ───────────────────────────────────────────────────────
    self.vis_cull_meshlets_pipeline = Pipeline::create(*self.device, {
        .definitions = {{"CULLING_MESHLET_COUNT", std::to_string(Model::MAX_MESHLET_INDICES)}},
        .module_name = "cull_meshlets",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "cull_meshlets.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.vis_cull_triangles_pipeline = Pipeline::create(*self.device, {
        .definitions = {{"CULLING_TRIANGLE_COUNT", std::to_string(Model::MAX_MESHLET_PRIMITIVES)}},
        .module_name = "cull_triangles",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "cull_triangles.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.vis_encode_pipeline = Pipeline::create(*self.device, {
        .module_name = "visbuffer_encode",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "visbuffer_encode.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    self.vis_decode_pipeline = Pipeline::create(*self.device, {
        .module_name = "visbuffer_decode",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "visbuffer_decode.slang",
        .entry_points = { "vs_main", "fs_main" },
    }, self.device->bindless_set()).value();
    //  ── PBR ─────────────────────────────────────────────────────────────
    self.pbr_basic_pipeline = Pipeline::create(*self.device, {
        .module_name = "brdf",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "brdf.slang",
        .entry_points = { "vs_main", "fs_main" },
    }, self.device->bindless_set()).value();
    //  ── POST PROCESS ────────────────────────────────────────────────────
    self.tonemap_pipeline = Pipeline::create(*self.device, {
        .module_name = "tonemap",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "tonemap.slang",
        .entry_points = { "vs_main", "fs_main" },
    }, self.device->bindless_set()).value();
    // clang-format on

    //  ── SKY LUTS ────────────────────────────────────────────────────────
    auto temp_atmos_info = GPU::Atmosphere{};
    temp_atmos_info.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
    temp_atmos_info.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
    auto temp_atmos = transfer_man.scratch_buffer(temp_atmos_info);

    auto transmittance_lut_pass = vuk::make_pass(
        "transmittance lut",
        [&pipeline = *self.device->pipeline(self.sky_transmittance_pipeline.id()),
         sky_transmittance_lut_index = self.sky_transmittance_lut_view.index(),
         &descriptor_set = self.device->bindless_set(
         )](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeRW) dst, VUK_BA(vuk::Access::eComputeRead) atmos) {
            cmd_list //
                .bind_compute_pipeline(pipeline)
                .bind_persistent(0, descriptor_set)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(atmos->device_address, sky_transmittance_lut_index))
                .dispatch_invocations_per_pixel(dst);

            return std::make_tuple(dst, atmos);
        }
    );

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(transmittance_lut_attachment, temp_atmos) = transmittance_lut_pass(std::move(transmittance_lut_attachment), std::move(temp_atmos));

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&pipeline = *self.device->pipeline(self.sky_multiscatter_pipeline.id()),
         &descriptor_set = self.device->bindless_set(),
         sampler_index = self.linear_clamp_sampler.index(),
         sky_transmittance_lut_index = self.sky_transmittance_lut_view.index(),
         sky_multiscattering_lut_index = self.sky_multiscatter_lut_view.index(
         )](vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
            VUK_IA(vuk::Access::eComputeRW) dst,
            VUK_BA(vuk::Access::eComputeRead) atmos) {
            cmd_list //
                .bind_compute_pipeline(pipeline)
                .bind_persistent(0, descriptor_set)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants(atmos->device_address, sampler_index, sky_transmittance_lut_index, sky_multiscattering_lut_index)
                )
                .dispatch_invocations_per_pixel(dst);

            return std::make_tuple(transmittance_lut, dst, atmos);
        }
    );

    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.discard(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment, temp_atmos) =
        multiscatter_lut_pass(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment), std::move(temp_atmos));

    transmittance_lut_attachment = transmittance_lut_attachment.as_released(vuk::Access::eComputeSampled, vuk::DomainFlagBits::eGraphicsQueue);
    multiscatter_lut_attachment = multiscatter_lut_attachment.as_released(vuk::Access::eComputeSampled, vuk::DomainFlagBits::eGraphicsQueue);
    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));

    self.device->set_name(self.sky_transmittance_lut, "Sky Transmittance LUT");
    self.device->set_name(self.sky_transmittance_lut_view, "Sky Transmittance LUT View");
    self.device->set_name(self.sky_multiscatter_lut, "Sky Multiscattering LUT");
    self.device->set_name(self.sky_multiscatter_lut_view, "Sky Multiscattering LUT View");
}

auto SceneRenderer::compose(this SceneRenderer &self, SceneComposeInfo &compose_info) -> ComposedScene {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();

    // IMPORTANT: Only wait when buffer is being resized!!!
    // We can still copy into gpu buffer if it has enough space.

    if (ls::size_bytes(compose_info.gpu_materials) > self.materials_buffer.data_size()) {
        if (self.materials_buffer) {
            self.device->wait();
            self.device->destroy(self.materials_buffer.id());
        }

        self.materials_buffer = Buffer::create(*self.device, ls::size_bytes(compose_info.gpu_materials)).value();
    }

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
    auto materials_buffer = //
        transfer_man.upload_staging(ls::span(compose_info.gpu_materials), self.materials_buffer);
    auto models_buffer = //
        transfer_man.upload_staging(ls::span(compose_info.gpu_models), self.models_buffer);
    auto meshlet_instances_buffer = //
        transfer_man.upload_staging(ls::span(compose_info.gpu_meshlet_instances), self.meshlet_instances_buffer);
    return ComposedScene{
        .materials_buffer = materials_buffer,
        .models_buffer = models_buffer,
        .meshlet_instances_buffer = meshlet_instances_buffer,
    };
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
    if (!info.dirty_transform_ids.empty()) {
        auto transform_count = rebuild_transforms ? info.transforms.size() : info.dirty_transform_ids.size();
        auto new_transforms_size_bytes = transform_count * sizeof(GPU::Transforms);

        auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, new_transforms_size_bytes);
        auto *dst_transform_ptr = reinterpret_cast<GPU::Transforms *>(upload_buffer->mapped_ptr);
        auto upload_offsets = std::vector<u64>(transform_count);

        if (!rebuild_transforms) {
            for (const auto &[dirty_transform_id, offset] : std::views::zip(info.dirty_transform_ids, upload_offsets)) {
                auto index = SlotMap_decode_id(dirty_transform_id).index;
                const auto &transform = info.transforms[index];
                std::memcpy(dst_transform_ptr, &transform, sizeof(GPU::Transforms));
                offset = index * sizeof(GPU::Transforms);
                dst_transform_ptr++;
            }
        } else {
            u64 offset = 0;
            for (auto &v : upload_offsets) {
                v = offset;
                offset += sizeof(GPU::Transforms);
            }

            std::memcpy(dst_transform_ptr, info.transforms.data(), ls::size_bytes(info.transforms));
        }

        auto update_transforms_pass = vuk::make_pass(
            "update scene transforms",
            [upload_offsets = std::move(upload_offsets
             )](vuk::CommandBuffer &cmd_list, VUK_BA(vuk::Access::eTransferRead) src_buffer, VUK_BA(vuk::Access::eTransferWrite) dst_buffer) {
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
        atmosphere_buffer = transfer_man.scratch_buffer(info.atmosphere.value());
    }

    auto camera_buffer = vuk::Value<vuk::Buffer>{};
    if (info.camera.has_value()) {
        camera_buffer = transfer_man.scratch_buffer(info.camera.value());
    }

    //  ── PREPARE ATTACHMENTS ─────────────────────────────────────────────
    auto final_attachment = vuk::declare_ia(
        "final",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage | vuk::ImageUsageFlagBits::eColorAttachment,
          .extent = info.extent,
          .format = vuk::Format::eB10G11R11UfloatPack32,
          .sample_count = vuk::Samples::e1,
          .level_count = 1,
          .layer_count = 1 }
    );
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia(
        "depth",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eDepthStencilAttachment,
          .extent = info.extent,
          .format = vuk::Format::eD32Sfloat,
          .sample_count = vuk::Samples::e1,
          .level_count = 1,
          .layer_count = 1 }
    );
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::ClearDepth(1.0));

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view
            .acquire(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.acquire(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);

    //  ── VISIBILITY BUFFER ───────────────────────────────────────────────
    auto visbuffer_attachment = vuk::declare_ia(
        "visbuffer",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR32Uint,
          .sample_count = vuk::Samples::e1 }
    );
    visbuffer_attachment.same_shape_as(final_attachment);
    visbuffer_attachment = vuk::clear_image(std::move(visbuffer_attachment), vuk::Clear(vuk::ClearColor(~0_u32, 0_u32, 0_u32, 0_u32)));

    auto albedo_attachment = vuk::declare_ia(
        "albedo",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR8G8B8A8Srgb,
          .sample_count = vuk::Samples::e1 }
    );
    albedo_attachment.same_shape_as(visbuffer_attachment);
    albedo_attachment = vuk::clear_image(std::move(albedo_attachment), vuk::Black<f32>);

    auto normal_attachment = vuk::declare_ia(
        "normal",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR16G16B16A16Sfloat,
          .sample_count = vuk::Samples::e1 }
    );
    normal_attachment.same_shape_as(visbuffer_attachment);
    normal_attachment = vuk::clear_image(std::move(normal_attachment), vuk::Black<f32>);

    auto emissive_attachment = vuk::declare_ia(
        "emissive",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eB10G11R11UfloatPack32,
          .sample_count = vuk::Samples::e1 }
    );
    emissive_attachment.same_shape_as(visbuffer_attachment);
    emissive_attachment = vuk::clear_image(std::move(emissive_attachment), vuk::Black<f32>);

    auto metallic_roughness_attachment = vuk::declare_ia(
        "metallic roughness",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR8G8B8A8Unorm,
          .sample_count = vuk::Samples::e1 }
    );
    metallic_roughness_attachment.same_shape_as(visbuffer_attachment);
    metallic_roughness_attachment = vuk::clear_image(std::move(metallic_roughness_attachment), vuk::Black<f32>);

    if (self.meshlet_instance_count) {
        vuk::Value<vuk::Buffer> materials_buffer;
        vuk::Value<vuk::Buffer> models_buffer;
        vuk::Value<vuk::Buffer> meshlet_instances_buffer;
        if (composed_scene.has_value()) {
            materials_buffer = std::move(composed_scene->materials_buffer);
            models_buffer = std::move(composed_scene->models_buffer);
            meshlet_instances_buffer = std::move(composed_scene->meshlet_instances_buffer);
        } else {
            materials_buffer = self.materials_buffer.acquire(*self.device, "Materials", vuk::Access::eNone);
            models_buffer = self.models_buffer.acquire(*self.device, "Models", vuk::Access::eNone);
            meshlet_instances_buffer = self.meshlet_instances_buffer.acquire(*self.device, "Meshlet Instances", vuk::Access::eNone);
        }

        //  ── CULL MESHLETS ───────────────────────────────────────────────────
        auto vis_cull_meshlets_pass = vuk::make_pass(
            "vis cull meshlets",
            [&pipeline = *self.device->pipeline(self.vis_cull_meshlets_pipeline.id()), meshlet_instance_count = self.meshlet_instance_count](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) scene,
                VUK_BA(vuk::eComputeWrite) meshlet_indirect,
                VUK_BA(vuk::eComputeWrite) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRead) models
            ) {
                cmd_list //
                    .bind_compute_pipeline(pipeline)
                    .bind_buffer(0, 0, scene)
                    .bind_buffer(0, 1, meshlet_indirect)
                    .bind_buffer(0, 2, visible_meshlet_instances_indices)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, transforms)
                    .bind_buffer(0, 5, models)
                    .dispatch((meshlet_instance_count + Model::MAX_MESHLET_INDICES - 1) / Model::MAX_MESHLET_INDICES);
                return std::make_tuple(scene, meshlet_indirect, visible_meshlet_instances_indices, meshlet_instances, transforms, models);
            }
        );

        auto meshlets_indirect_dispatch = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
        auto visible_meshlet_instances_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * sizeof(u32));

        //  ── CULL TRIANGLES ──────────────────────────────────────────────────
        auto vis_cull_triangles_pass = vuk::make_pass(
            "vis cull triangles",
            [&pipeline = *self.device->pipeline(self.vis_cull_triangles_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eIndirectRead) meshlet_indirect,
                VUK_BA(vuk::eComputeRead) scene,
                VUK_BA(vuk::eComputeWrite) triangles_indirect,
                VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) models,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeWrite) reordered_indices) {
                cmd_list //
                    .bind_compute_pipeline(pipeline)
                    .bind_buffer(0, 0, scene)
                    .bind_buffer(0, 1, triangles_indirect)
                    .bind_buffer(0, 2, visible_meshlet_instances_indices)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, models)
                    .bind_buffer(0, 5, transforms)
                    .bind_buffer(0, 6, reordered_indices)
                    .dispatch_indirect(meshlet_indirect);
                return std::make_tuple(
                    scene,
                    triangles_indirect,
                    visible_meshlet_instances_indices,
                    meshlet_instances,
                    models,
                    transforms,
                    reordered_indices
                );
            }
        );

        auto triangles_indexed_indirect_dispatch = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });
        auto reordered_indices_buffer = transfer_man.alloc_transient_buffer(
            vuk::MemoryUsage::eGPUonly,
            self.meshlet_instance_count * Model::MAX_MESHLET_PRIMITIVES * 3 * sizeof(u32)
        );

        //  ── VISBUFFER ENCODE ────────────────────────────────────────────────
        auto vis_encode_pass = vuk::make_pass(
            "vis encode",
            [&pipeline = *self.device->pipeline(self.vis_encode_pipeline.id()), &descriptor_set = self.device->bindless_set()](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) dst,
                VUK_IA(vuk::eDepthStencilRW) dst_depth,
                VUK_BA(vuk::eIndirectRead) triangle_indirect,
                VUK_BA(vuk::eVertexRead) scene,
                VUK_BA(vuk::eVertexRead) transforms,
                VUK_BA(vuk::eVertexRead) models,
                VUK_BA(vuk::eVertexRead) meshlet_instances,
                VUK_BA(vuk::eVertexRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eFragmentRead) materials,
                VUK_BA(vuk::eIndexRead) index_buffer
            ) {
                cmd_list //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                    .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(dst, vuk::BlendPreset::eOff)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, scene)
                    .bind_buffer(0, 1, transforms)
                    .bind_buffer(0, 2, models)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, visible_meshlet_instances_indices)
                    .bind_buffer(0, 5, materials)
                    .bind_persistent(1, descriptor_set)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                    .draw_indexed_indirect(1, triangle_indirect);
                return std::make_tuple(dst, dst_depth, scene, transforms, models, meshlet_instances, visible_meshlet_instances_indices, materials);
            }
        );

        //  ── VISBUFFER DECODE ────────────────────────────────────────────────
        auto vis_decode_pass = vuk::make_pass(
            "vis decode",
            [&pipeline = *self.device->pipeline(self.vis_decode_pipeline.id()), &descriptor_set = self.device->bindless_set()](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) albedo,
                VUK_IA(vuk::eColorWrite) normal,
                VUK_IA(vuk::eColorWrite) emissive,
                VUK_IA(vuk::eColorWrite) metallic_roughness,
                VUK_IA(vuk::eFragmentRead) visbuffer,
                VUK_BA(vuk::eFragmentRead) scene,
                VUK_BA(vuk::eFragmentRead) transforms,
                VUK_BA(vuk::eFragmentRead) models,
                VUK_BA(vuk::eFragmentRead) meshlet_instances,
                VUK_BA(vuk::eFragmentRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eFragmentRead) materials
            ) {
                cmd_list //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                    .set_depth_stencil({})
                    .set_color_blend(albedo, vuk::BlendPreset::eOff)
                    .set_color_blend(normal, vuk::BlendPreset::eOff)
                    .set_color_blend(emissive, vuk::BlendPreset::eOff)
                    .set_color_blend(metallic_roughness, vuk::BlendPreset::eOff)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_image(0, 0, visbuffer)
                    .bind_buffer(0, 1, scene)
                    .bind_buffer(0, 2, transforms)
                    .bind_buffer(0, 3, models)
                    .bind_buffer(0, 4, meshlet_instances)
                    .bind_buffer(0, 5, visible_meshlet_instances_indices)
                    .bind_buffer(0, 6, materials)
                    .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, glm::vec2(visbuffer->extent.width, visbuffer->extent.height))
                    .draw(3, 1, 0, 1);
                return std::make_tuple(albedo, normal, emissive, metallic_roughness, scene, transforms);
            }
        );
    }

    //  ── PBR BASIC ───────────────────────────────────────────────────────
    auto pbr_basic_pass = vuk::make_pass(
        "pbr basic",
        [&pipeline = *self.device->pipeline(self.pbr_basic_pipeline.id()
         )](vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_BA(vuk::Access::eFragmentRead) scene,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut,
            VUK_IA(vuk::Access::eFragmentRead) depth,
            VUK_IA(vuk::Access::eFragmentSampled) albedo,
            VUK_IA(vuk::Access::eFragmentRead) normal,
            VUK_IA(vuk::Access::eFragmentRead) emissive,
            VUK_IA(vuk::Access::eFragmentRead) metallic_roughness) {
            cmd_list //
                .bind_graphics_pipeline(pipeline)
                .bind_buffer(0, 0, scene)
                .set_rasterization({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, scene, transmittance_lut, multiscatter_lut, depth);
        }
    );

    if (info.atmosphere.has_value()) {
        auto sky_view_lut_attachment = vuk::declare_ia(
            "sky_view_lut",
            { .image_type = vuk::ImageType::e2D,
              .extent = self.sky_view_lut_extent,
              .sample_count = vuk::Samples::e1,
              .view_type = vuk::ImageViewType::e2D,
              .level_count = 1,
              .layer_count = 1 }
        );
        sky_view_lut_attachment.same_format_as(final_attachment);

        //  ── SKY VIEW LUT ────────────────────────────────────────────────────
        auto sky_view_pass = vuk::make_pass(
            "sky view",
            [&pipeline = *self.device->pipeline(self.sky_view_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eComputeRW) dst,
                VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
                VUK_IA(vuk::Access::eComputeSampled) multiscatter_lut,
                VUK_BA(vuk::Access::eComputeRead) scene) {
                vuk::SamplerCreateInfo sampler_info = {
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                };

                cmd_list //
                    .bind_compute_pipeline(pipeline)
                    .bind_sampler(0, 0, sampler_info)
                    .bind_image(0, 1, dst)
                    .bind_image(0, 2, transmittance_lut)
                    .bind_image(0, 3, multiscatter_lut)
                    .bind_buffer(0, 4, scene)
                    .dispatch_invocations_per_pixel(dst);
                return std::make_tuple(dst, transmittance_lut, multiscatter_lut, scene);
            }
        );

        //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
        auto sky_aerial_perspective_pass = vuk::make_pass(
            "sky aerial perspective",
            [&pipeline = *self.device->pipeline(self.sky_aerial_perspective_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eComputeWrite) dst,
                VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
                VUK_IA(vuk::Access::eComputeSampled) multiscatter_lut,
                VUK_BA(vuk::Access::eComputeRead) scene) {
                vuk::SamplerCreateInfo sampler_info = {
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                };

                cmd_list //
                    .bind_compute_pipeline(pipeline)
                    .bind_sampler(0, 0, sampler_info)
                    .bind_image(0, 1, dst)
                    .bind_image(0, 2, transmittance_lut)
                    .bind_image(0, 3, multiscatter_lut)
                    .bind_buffer(0, 4, scene)
                    .dispatch_invocations_per_pixel(dst);
                return std::make_tuple(dst, transmittance_lut, multiscatter_lut, scene);
            }
        );

        auto sky_aerial_perspective_attachment = vuk::declare_ia(
            "sky_aerial_perspective",
            { .image_type = vuk::ImageType::e3D,
              .extent = self.sky_aerial_perspective_lut_extent,
              .sample_count = vuk::Samples::e1,
              .view_type = vuk::ImageViewType::e3D,
              .level_count = 1,
              .layer_count = 1 }
        );
        sky_aerial_perspective_attachment.same_format_as(final_attachment);

        //  ── SKY FINAL ───────────────────────────────────────────────────────
        auto sky_final_pass = vuk::make_pass(
            "sky final",
            [&pipeline = *self.device->pipeline(self.sky_final_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eColorWrite) dst,
                VUK_IA(vuk::Access::eFragmentRead) depth,
                VUK_IA(vuk::Access::eFragmentSampled) sky_view_lut,
                VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
                VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
                VUK_BA(vuk::Access::eFragmentRead) scene) {
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
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({})
                    .set_depth_stencil({})
                    .set_color_blend(dst, blend_info)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .draw(3, 1, 0, 0);
                return std::make_tuple(dst, depth, sky_view_lut, aerial_perspective_lut, transmittance_lut, scene);
            }
        );
    }

    auto result_attachment = vuk::declare_ia(
        "result",
        { .usage = vuk::ImageUsageFlagBits::eStorage | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR8G8B8A8Srgb,
          .sample_count = vuk::Samples::e1,
          .layer_count = 1 }
    );
    result_attachment.same_shape_as(final_attachment);

    //  ── TONEMAP ─────────────────────────────────────────────────────────
    auto tonemap_pass = vuk::make_pass(
        "tonemap",
        [&pipeline = *self.device->pipeline(self.tonemap_pipeline.id()
         )](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst, VUK_IA(vuk::Access::eFragmentSampled) src) {
            cmd_list //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_image(0, 0, src)
                .bind_sampler(0, 1, {})
                .draw(3, 1, 0, 0);
            return dst;
        }
    );

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [&pipeline = *self.device->pipeline(self.grid_pipeline.id()
         )](vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eDepthStencilRW) depth,
            VUK_BA(vuk::Access::eVertexRead | vuk::Access::eFragmentRead) scene) {
            cmd_list //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_buffer(0, 0, scene)
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth, scene);
        }
    );

    return result_attachment;
}

} // namespace lr
