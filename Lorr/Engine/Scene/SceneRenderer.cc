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

    auto linear_repeat_sampler_info = SamplerInfo{
        .min_filter = vuk::Filter::eLinear,
        .mag_filter = vuk::Filter::eLinear,
        .mipmap_mode = vuk::SamplerMipmapMode::eLinear,
        .addr_u = vuk::SamplerAddressMode::eRepeat,
        .addr_v = vuk::SamplerAddressMode::eRepeat,
        .addr_w = vuk::SamplerAddressMode::eRepeat,
        .compare_op = vuk::CompareOp::eNever,
    };
    self.linear_repeat_sampler = Sampler::create(*self.device, linear_repeat_sampler_info).value();

    auto linear_clamp_sampler_info = SamplerInfo{
        .min_filter = vuk::Filter::eLinear,
        .mag_filter = vuk::Filter::eLinear,
        .mipmap_mode = vuk::SamplerMipmapMode::eLinear,
        .addr_u = vuk::SamplerAddressMode::eClampToEdge,
        .addr_v = vuk::SamplerAddressMode::eClampToEdge,
        .addr_w = vuk::SamplerAddressMode::eClampToEdge,
        .compare_op = vuk::CompareOp::eNever,
    };
    self.linear_clamp_sampler = Sampler::create(*self.device, linear_clamp_sampler_info).value();

    auto nearest_repeat_sampler_info = SamplerInfo{
        .min_filter = vuk::Filter::eNearest,
        .mag_filter = vuk::Filter::eNearest,
        .mipmap_mode = vuk::SamplerMipmapMode::eLinear,
        .addr_u = vuk::SamplerAddressMode::eRepeat,
        .addr_v = vuk::SamplerAddressMode::eRepeat,
        .addr_w = vuk::SamplerAddressMode::eRepeat,
        .compare_op = vuk::CompareOp::eNever,
    };
    self.nearest_repeat_sampler = Sampler::create(*self.device, nearest_repeat_sampler_info).value();

    BindlessDescriptorInfo bindless_set_info[] = {
        { .binding = BindlessDescriptorLayout::Samplers, .type = vuk::DescriptorType::eSampler, .descriptor_count = 1 },
        { .binding = BindlessDescriptorLayout::SampledImages, .type = vuk::DescriptorType::eSampledImage, .descriptor_count = 1024 },
    };
    self.bindless_set = self.device->create_persistent_descriptor_set(bindless_set_info, 1).release();

    self.bindless_set.update_sampler(BindlessDescriptorLayout::Samplers, 0, *self.device->sampler(self.linear_repeat_sampler.id()));
    self.device->commit_descriptor_set(self.bindless_set);

    //  ── GRID ────────────────────────────────────────────────────────────
    auto grid_pipeline_info = ShaderCompileInfo{
        .module_name = "editor.grid",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "editor_grid.slang",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.grid_pipeline = Pipeline::create(*self.device, grid_pipeline_info).value();

    //  ── SKY ─────────────────────────────────────────────────────────────
    auto sky_transmittance_lut_info = ImageInfo{
        .format = vuk::Format::eR16G16B16A16Sfloat,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 256, .height = 64, .depth = 1 },
        .name = "Sky Transmittance",
    };
    std::tie(self.sky_transmittance_lut, self.sky_transmittance_lut_view) = Image::create_with_view(*self.device, sky_transmittance_lut_info).value();
    auto sky_transmittance_pipeline_info = ShaderCompileInfo{
        .module_name = "sky_transmittance",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_transmittance.slang",
        .entry_points = { "cs_main" },
    };
    self.sky_transmittance_pipeline = Pipeline::create(*self.device, sky_transmittance_pipeline_info).value();

    auto sky_multiscatter_lut_info = ImageInfo{
        .format = vuk::Format::eR16G16B16A16Sfloat,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 32, .height = 32, .depth = 1 },
        .name = "Sky Multiscatter LUT",
    };
    std::tie(self.sky_multiscatter_lut, self.sky_multiscatter_lut_view) = Image::create_with_view(*self.device, sky_multiscatter_lut_info).value();
    auto sky_multiscatter_pipeline_info = ShaderCompileInfo{
        .module_name = "sky_multiscattering",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_multiscattering.slang",
        .entry_points = { "cs_main" },
    };
    self.sky_multiscatter_pipeline = Pipeline::create(*self.device, sky_multiscatter_pipeline_info).value();

    auto sky_view_pipeline_info = ShaderCompileInfo{
        .module_name = "sky_view",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_view.slang",
        .entry_points = { "cs_main" },
    };
    self.sky_view_pipeline = Pipeline::create(*self.device, sky_view_pipeline_info).value();

    auto sky_aerial_perspective_pipeline_info = ShaderCompileInfo{
        .module_name = "sky_aerial_perspective",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_aerial_perspective.slang",
        .entry_points = { "cs_main" },
    };
    self.sky_aerial_perspective_pipeline = Pipeline::create(*self.device, sky_aerial_perspective_pipeline_info).value();

    auto sky_final_pipeline_info = ShaderCompileInfo{
        .module_name = "sky_final",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "sky_final.slang",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.sky_final_pipeline = Pipeline::create(*self.device, sky_final_pipeline_info).value();

    //  ── VISBUFFER ───────────────────────────────────────────────────────
    auto vis_cull_meshlets_pipeline_info = ShaderCompileInfo{
        .definitions = { { "CULLING_MESHLET_COUNT", std::to_string(Model::MAX_MESHLET_INDICES) } },
        .module_name = "cull_meshlets",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "cull_meshlets.slang",
        .entry_points = { "cs_main" },
    };
    self.vis_cull_meshlets_pipeline = Pipeline::create(*self.device, vis_cull_meshlets_pipeline_info).value();

    auto vis_cull_triangles_pipeline_info = ShaderCompileInfo{
        .definitions = { { "CULLING_TRIANGLE_COUNT", std::to_string(Model::MAX_MESHLET_PRIMITIVES) } },
        .module_name = "cull_triangles",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "cull_triangles.slang",
        .entry_points = { "cs_main" },
    };
    self.vis_cull_triangles_pipeline = Pipeline::create(*self.device, vis_cull_triangles_pipeline_info).value();

    auto vis_encode_pipeline_info = ShaderCompileInfo{
        .module_name = "visbuffer_encode",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "visbuffer_encode.slang",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.vis_encode_pipeline = Pipeline::create(*self.device, vis_encode_pipeline_info, self.bindless_set).value();

    auto vis_decode_pipeline_info = ShaderCompileInfo{
        .module_name = "visbuffer_decode",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "visbuffer_decode.slang",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.vis_decode_pipeline = Pipeline::create(*self.device, vis_decode_pipeline_info, self.bindless_set).value();

    //  ── PBR ─────────────────────────────────────────────────────────────
    auto pbr_basic_pipeline_info = ShaderCompileInfo{
        .module_name = "brdf",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "brdf.slang",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.pbr_basic_pipeline = Pipeline::create(*self.device, pbr_basic_pipeline_info).value();

    //  ── POST PROCESS ────────────────────────────────────────────────────
    auto tonemap_pipeline_info = ShaderCompileInfo{
        .module_name = "tonemap",
        .root_path = shaders_root,
        .shader_path = shaders_root / "passes" / "tonemap.slang",
        .entry_points = { "vs_main", "fs_main" },
    };
    self.tonemap_pipeline = Pipeline::create(*self.device, tonemap_pipeline_info).value();

    //  ── SKY LUTS ────────────────────────────────────────────────────────
    auto temp_atmos_info = GPU::Atmosphere{};
    temp_atmos_info.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
    temp_atmos_info.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
    auto temp_atmos = transfer_man.scratch_buffer(temp_atmos_info);

    auto transmittance_lut_pass = vuk::make_pass(
        "transmittance lut",
        [&pipeline = *self.device->pipeline(self.sky_transmittance_pipeline.id()
         )](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::eComputeRW) dst, VUK_BA(vuk::eComputeRead) atmos) {
            cmd_list //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, dst)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(atmos->device_address))
                .dispatch_invocations_per_pixel(dst);

            return std::make_tuple(dst, atmos);
        }
    );

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(transmittance_lut_attachment, temp_atmos) = transmittance_lut_pass(std::move(transmittance_lut_attachment), std::move(temp_atmos));

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&pipeline = *self.device->pipeline(self.sky_multiscatter_pipeline.id()), sampler = *self.device->sampler(self.linear_clamp_sampler.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
            VUK_IA(vuk::eComputeRW) sky_multiscatter_lut,
            VUK_BA(vuk::eComputeRead) atmos
        ) {
            cmd_list //
                .bind_compute_pipeline(pipeline)
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
}

auto SceneRenderer::compose(this SceneRenderer &self, SceneComposeInfo &compose_info) -> ComposedScene {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();

    for (u32 i = 0; i < compose_info.rendering_image_view_ids.size(); i++) {
        auto *image_view = self.device->image_view(compose_info.rendering_image_view_ids[i]);
        self.bindless_set.update_sampled_image(BindlessDescriptorLayout::SampledImages, i, *image_view, vuk::ImageLayout::eReadOnlyOptimal);
    }
    self.device->commit_descriptor_set(self.bindless_set);

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

    //   ──────────────────────────────────────────────────────────────────────
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

    auto metallic_roughness_occlusion_attachment = vuk::declare_ia(
        "metallic roughness occlusion",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR8G8B8A8Unorm,
          .sample_count = vuk::Samples::e1 }
    );
    metallic_roughness_occlusion_attachment.same_shape_as(visbuffer_attachment);
    metallic_roughness_occlusion_attachment = vuk::clear_image(std::move(metallic_roughness_occlusion_attachment), vuk::Black<f32>);

    auto sky_view_lut_attachment = vuk::declare_ia(
        "sky view lut",
        { .image_type = vuk::ImageType::e2D,
          .extent = self.sky_view_lut_extent,
          .format = vuk::Format::eR16G16B16A16Sfloat,
          .sample_count = vuk::Samples::e1,
          .view_type = vuk::ImageViewType::e2D,
          .level_count = 1,
          .layer_count = 1 }
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
    sky_aerial_perspective_attachment.same_format_as(sky_view_lut_attachment);

    auto sky_transmittance_lut_attachment =
        self.sky_transmittance_lut_view
            .acquire(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
    auto sky_multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.acquire(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);

    //   ──────────────────────────────────────────────────────────────────────

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
            [&pipeline = *self.device->pipeline(self.vis_cull_meshlets_pipeline.id()),
             meshlet_instance_count = self.meshlet_instance_count,
             cull_flags = info.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeWrite) cull_triangles_cmd,
                VUK_BA(vuk::eComputeWrite) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRead) models,
                VUK_BA(vuk::eComputeRead) camera
            ) {
                cmd_list //
                    .bind_compute_pipeline(pipeline)
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
        auto visible_meshlet_instances_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * sizeof(u32));

        std::tie(
            cull_triangles_cmd_buffer,
            visible_meshlet_instances_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            models_buffer,
            camera_buffer
        ) =
            vis_cull_meshlets_pass(
                std::move(cull_triangles_cmd_buffer),
                std::move(visible_meshlet_instances_buffer),
                std::move(meshlet_instances_buffer),
                std::move(transforms_buffer),
                std::move(models_buffer),
                std::move(camera_buffer)
            );

        //  ── CULL TRIANGLES ──────────────────────────────────────────────────
        auto draw_indexed_cmd_buffer = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });
        auto reordered_indices_buffer = transfer_man.alloc_transient_buffer(
            vuk::MemoryUsage::eGPUonly,
            self.meshlet_instance_count * Model::MAX_MESHLET_PRIMITIVES * 3 * sizeof(u32)
        );

        auto vis_cull_triangles_pass = vuk::make_pass(
            "vis cull triangles",
            [&pipeline = *self.device->pipeline(self.vis_cull_triangles_pipeline.id()), cull_flags = info.cull_flags](
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
                    .bind_compute_pipeline(pipeline)
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

        std::tie(
            draw_indexed_cmd_buffer,
            visible_meshlet_instances_buffer,
            reordered_indices_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            models_buffer,
            camera_buffer
        ) =
            vis_cull_triangles_pass(
                std::move(cull_triangles_cmd_buffer),
                std::move(draw_indexed_cmd_buffer),
                std::move(visible_meshlet_instances_buffer),
                std::move(reordered_indices_buffer),
                std::move(meshlet_instances_buffer),
                std::move(transforms_buffer),
                std::move(models_buffer),
                std::move(camera_buffer)
            );

        //  ── VISBUFFER ENCODE ────────────────────────────────────────────────
        auto vis_encode_pass = vuk::make_pass(
            "vis encode",
            [&pipeline = *self.device->pipeline(self.vis_encode_pipeline.id()), &descriptor_set = self.bindless_set](
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
                VUK_BA(vuk::eFragmentRead) materials
            ) {
                cmd_list //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                    .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(visbuffer, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, visible_meshlet_instances_indices)
                    .bind_buffer(0, 2, meshlet_instances)
                    .bind_buffer(0, 3, models)
                    .bind_buffer(0, 4, transforms)
                    .bind_buffer(0, 5, materials)
                    .bind_persistent(1, descriptor_set)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                    .draw_indexed_indirect(1, triangle_indirect);
                return std::make_tuple(visbuffer, depth, camera, visible_meshlet_instances_indices, meshlet_instances, transforms, models, materials);
            }
        );

        std::tie(
            visbuffer_attachment,
            depth_attachment,
            camera_buffer,
            visible_meshlet_instances_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            models_buffer,
            materials_buffer
        ) =
            vis_encode_pass(
                std::move(draw_indexed_cmd_buffer),
                std::move(reordered_indices_buffer),
                std::move(visbuffer_attachment),
                std::move(depth_attachment),
                std::move(camera_buffer),
                std::move(visible_meshlet_instances_buffer),
                std::move(meshlet_instances_buffer),
                std::move(transforms_buffer),
                std::move(models_buffer),
                std::move(materials_buffer)
            );

        //  ── VISBUFFER DECODE ────────────────────────────────────────────────
        auto vis_decode_pass = vuk::make_pass(
            "vis decode",
            [&pipeline = *self.device->pipeline(self.vis_decode_pipeline.id()), &descriptor_set = self.bindless_set](
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
                    .bind_graphics_pipeline(pipeline)
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
                    .bind_persistent(1, descriptor_set)
                    .draw(3, 1, 0, 1);
                return std::make_tuple(albedo, normal, emissive, metallic_roughness_occlusion, camera, transforms);
            }
        );

        std::tie(
            albedo_attachment,
            normal_attachment,
            emissive_attachment,
            metallic_roughness_occlusion_attachment,
            camera_buffer,
            transforms_buffer
        ) =
            vis_decode_pass(
                std::move(albedo_attachment),
                std::move(normal_attachment),
                std::move(emissive_attachment),
                std::move(metallic_roughness_occlusion_attachment),
                std::move(camera_buffer),
                std::move(visible_meshlet_instances_buffer),
                std::move(meshlet_instances_buffer),
                std::move(models_buffer),
                std::move(transforms_buffer),
                std::move(materials_buffer),
                std::move(visbuffer_attachment)
            );

        //  ── BRDF ────────────────────────────────────────────────────────────
        auto brdf_pass = vuk::make_pass(
            "brdf",
            [&pipeline = *self.device->pipeline(self.pbr_basic_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
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
                VUK_IA(vuk::eFragmentRead) metallic_roughness_occlusion) {
                auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                };

                auto nearest_repeat_sampler = vuk::SamplerCreateInfo{
                    .magFilter = vuk::Filter::eNearest,
                    .minFilter = vuk::Filter::eNearest,
                    .addressModeU = vuk::SamplerAddressMode::eRepeat,
                    .addressModeV = vuk::SamplerAddressMode::eRepeat,
                };

                cmd_list //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({})
                    .set_color_blend(dst, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_sampler(0, 0, linear_clamp_sampler)
                    .bind_sampler(0, 1, nearest_repeat_sampler)
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
    }

    if (info.atmosphere.has_value()) {
        //  ── SKY VIEW LUT ────────────────────────────────────────────────────
        auto sky_view_pass = vuk::make_pass(
            "sky view",
            [&pipeline = *self.device->pipeline(self.sky_view_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) atmosphere,
                VUK_BA(vuk::eComputeRead) sun,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                VUK_IA(vuk::eComputeRW) sky_view_lut) {
                auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                };

                cmd_list //
                    .bind_compute_pipeline(pipeline)
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
            [&pipeline = *self.device->pipeline(self.sky_aerial_perspective_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) atmosphere,
                VUK_BA(vuk::eComputeRead) sun,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                VUK_IA(vuk::eComputeWrite) sky_aerial_perspective_lut) {
                auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                    .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                };

                cmd_list //
                    .bind_compute_pipeline(pipeline)
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
            [&pipeline = *self.device->pipeline(self.sky_final_pipeline.id()
             )](vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) dst,
                VUK_BA(vuk::eFragmentRead) atmosphere,
                VUK_BA(vuk::eFragmentRead) sun,
                VUK_BA(vuk::eFragmentRead) camera,
                VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
                VUK_IA(vuk::eFragmentSampled) sky_aerial_perspective_lut,
                VUK_IA(vuk::eFragmentSampled) sky_view_lut,
                VUK_IA(vuk::eFragmentRead) depth) {
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
                    .bind_graphics_pipeline(pipeline)
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

    auto result_attachment = vuk::declare_ia(
        "result",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = info.format,
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
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, src)
                .draw(3, 1, 0, 0);
            return dst;
        }
    );
    result_attachment = tonemap_pass(std::move(result_attachment), std::move(final_attachment));

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [&pipeline = *self.device->pipeline(self.grid_pipeline.id()
         )](vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::eColorWrite) dst,
            VUK_IA(vuk::eDepthStencilRW) depth,
            VUK_BA(vuk::eVertexRead | vuk::eFragmentRead) camera) {
            cmd_list //
                .bind_graphics_pipeline(pipeline)
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
    std::tie(result_attachment, depth_attachment) =
        editor_grid_pass(std::move(result_attachment), std::move(depth_attachment), std::move(camera_buffer));

    return result_attachment;
}

} // namespace lr
