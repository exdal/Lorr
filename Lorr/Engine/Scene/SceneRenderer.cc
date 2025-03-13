#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Core/Application.hh"

namespace lr {
enum : u32 {
    Descriptor_Images = 0,
    Descriptor_Sampler,
};

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
    BindlessDescriptorInfo descriptor_infos[] = {
        { .binding = Descriptor_Images, .type = vuk::DescriptorType::eSampledImage, .descriptor_count = 1024 },
        { .binding = Descriptor_Sampler, .type = vuk::DescriptorType::eSampler, .descriptor_count = 1024 },
    };
    self.bindless_set = self.device->create_persistent_descriptor_set(descriptor_infos, 1).release();

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
        .definitions = {{"ATMOS_NO_EVAL_MS", "1"}},
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
        self.cloud_noise_lut_extent)
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
        self.cloud_noise_lut_extent)
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
    self.vis_cull_meshlets_pipeline = Pipeline::create(*self.device, {
        .definitions = {{"CULLING_MESHLET_COUNT", std::to_string(Model::MAX_MESHLET_INDICES)}},
        .module_name = "vis.cull_meshlets",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "cull_meshlets.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.vis_cull_triangles_pipeline = Pipeline::create(*self.device, {
        .definitions = {{"CULLING_TRIANGLE_COUNT", std::to_string(Model::MAX_MESHLET_PRIMITIVES)}},
        .module_name = "vis.cull_triangles",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "cull_triangles.slang",
        .entry_points = { "cs_main" },
    }).value();
    self.vis_encode_pipeline = Pipeline::create(*self.device, {
        .module_name = "vis.encode",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "encode.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    self.vis_decode_pipeline = Pipeline::create(*self.device, {
        .module_name = "vis.decode",
        .root_path = shaders_root,
        .shader_path = shaders_root / "vis" / "decode.slang",
        .entry_points = { "vs_main", "fs_main" },
    }, self.bindless_set).value();
    //  ── PBR ─────────────────────────────────────────────────────────────
    self.pbr_basic_pipeline = Pipeline::create(*self.device, {
        .module_name = "pbr.basic",
        .root_path = shaders_root,
        .shader_path = shaders_root / "pbr" / "basic.slang",
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
        });

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&pipeline = *self.device->pipeline(self.sky_multiscatter_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
            VUK_IA(vuk::Access::eComputeRW) dst,
            VUK_BA(vuk::Access::eComputeRead) scene) {
            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, dst)
                .bind_buffer(0, 3, scene)
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst, scene);
        });

    auto transmittance_lut_attachment =
        self.sky_transmittance_lut_view.discard(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.discard(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eStorage);

    auto temp_scene_info = GPU::Scene{};
    temp_scene_info.atmosphere.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
    temp_scene_info.atmosphere.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
    temp_scene_info.clouds.noise_lut_size = self.cloud_noise_lut_extent;
    auto temp_scene = transfer_man.scratch_buffer(temp_scene_info);

    std::tie(transmittance_lut_attachment, temp_scene) =
        transmittance_lut_pass(std::move(transmittance_lut_attachment), std::move(temp_scene));
    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment, temp_scene) =
        multiscatter_lut_pass(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment), std::move(temp_scene));

    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));

    //  ── CLOUD LUTS ──────────────────────────────────────────────────────
    auto cloud_noise_lut_pass = vuk::make_pass(
        "cloud noise pass",
        [&pipeline = *self.device->pipeline(self.cloud_noise_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeRW) shape_noise_lut,
            VUK_IA(vuk::Access::eComputeRW) detail_noise_lut,
            VUK_BA(vuk::Access::eComputeRead) scene) {
            auto image_size = glm::vec2(shape_noise_lut->extent.width, shape_noise_lut->extent.height);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, shape_noise_lut)
                .bind_image(0, 1, detail_noise_lut)
                .bind_buffer(0, 2, scene)
                .dispatch(groups.x, groups.y, shape_noise_lut->extent.depth);
            return std::make_tuple(shape_noise_lut, detail_noise_lut, scene);
        });

    auto cloud_shape_noise_lut_attachment =
        self.cloud_shape_noise_lut_view.discard(*self.device, "cloud shape lut", vuk::ImageUsageFlagBits::eStorage);
    auto cloud_detail_noise_lut_attachment =
        self.cloud_detail_noise_lut_view.discard(*self.device, "cloud detail lut", vuk::ImageUsageFlagBits::eStorage);

    std::tie(cloud_shape_noise_lut_attachment, cloud_detail_noise_lut_attachment, temp_scene) = cloud_noise_lut_pass(
        std::move(cloud_shape_noise_lut_attachment), std::move(cloud_detail_noise_lut_attachment), std::move(temp_scene));
    transfer_man.wait_on(std::move(cloud_shape_noise_lut_attachment));
    transfer_man.wait_on(std::move(cloud_detail_noise_lut_attachment));

    self.device->set_name(self.sky_transmittance_lut, "Sky Transmittance LUT");
    self.device->set_name(self.sky_transmittance_lut_view, "Sky Transmittance LUT View");
    self.device->set_name(self.sky_multiscatter_lut, "Sky Multiscattering LUT");
    self.device->set_name(self.sky_multiscatter_lut_view, "Sky Multiscattering LUT View");
    self.device->set_name(self.cloud_shape_noise_lut, "Cloud Shape Noise");
    self.device->set_name(self.cloud_shape_noise_lut_view, "Cloud Shape Noise View");
    self.device->set_name(self.cloud_detail_noise_lut, "Cloud Detail Noise");
    self.device->set_name(self.cloud_detail_noise_lut_view, "Cloud Detail Noise View");
}

auto SceneRenderer::compose(this SceneRenderer &self, SceneComposeInfo &compose_info) -> ComposedScene {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();

    for (u32 i = 0; i < compose_info.image_view_ids.size(); i++) {
        auto *image_view = self.device->image_view(compose_info.image_view_ids[i]);
        auto *sampler = self.device->sampler(compose_info.samplers[i]);
        self.bindless_set.update_sampled_image(Descriptor_Images, i, *image_view, vuk::ImageLayout::eReadOnlyOptimal);
        self.bindless_set.update_sampler(Descriptor_Sampler, i, *sampler);
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

    if (ls::size_bytes(compose_info.gpu_meshes) > self.meshes_buffer.data_size()) {
        if (self.meshes_buffer) {
            self.device->wait();
            self.device->destroy(self.meshes_buffer.id());
        }

        self.meshes_buffer = Buffer::create(*self.device, ls::size_bytes(compose_info.gpu_meshes)).value();
    }

    if (ls::size_bytes(compose_info.gpu_meshlet_instances) > self.meshlet_instances_buffer.data_size()) {
        if (self.meshlet_instances_buffer) {
            self.device->wait();
            self.device->destroy(self.meshlet_instances_buffer.id());
        }

        self.meshlet_instances_buffer = Buffer::create(*self.device, ls::size_bytes(compose_info.gpu_meshlet_instances)).value();
    }

    self.meshlet_instance_count = compose_info.gpu_meshlet_instances.size();
    auto materials_buffer =  //
        transfer_man.upload_staging(ls::span(compose_info.gpu_materials), self.materials_buffer);
    auto meshes_buffer =  //
        transfer_man.upload_staging(ls::span(compose_info.gpu_meshes), self.meshes_buffer);
    auto meshlet_instances_buffer =  //
        transfer_man.upload_staging(ls::span(compose_info.gpu_meshlet_instances), self.meshlet_instances_buffer);
    return ComposedScene{
        .materials_buffer = materials_buffer,
        .meshes_buffer = meshes_buffer,
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
            [upload_offsets = std::move(upload_offsets)](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::Access::eTransferRead) src_buffer,
                VUK_BA(vuk::Access::eTransferWrite) dst_buffer) {
                for (usize i = 0; i < upload_offsets.size(); i++) {
                    auto offset = upload_offsets[i];
                    auto src_subrange = src_buffer->subrange(i * sizeof(GPU::Transforms), sizeof(GPU::Transforms));
                    auto dst_subrange = dst_buffer->subrange(offset, sizeof(GPU::Transforms));
                    cmd_list.copy_buffer(src_subrange, dst_subrange);
                }

                return dst_buffer;
            });

        transforms_buffer = update_transforms_pass(std::move(upload_buffer), std::move(transforms_buffer));
    }

    info.scene_info.atmosphere.sky_view_lut_size = self.sky_view_lut_extent;
    info.scene_info.atmosphere.aerial_perspective_lut_size = self.sky_aerial_perspective_lut_extent;
    info.scene_info.atmosphere.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
    info.scene_info.atmosphere.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
    info.scene_info.clouds.noise_lut_size = self.cloud_shape_noise_lut_view.extent();
    info.scene_info.meshlet_instance_count = self.meshlet_instance_count;
    auto scene_buffer = transfer_man.scratch_buffer(info.scene_info);

    //  ── PREPARE ATTACHMENTS ─────────────────────────────────────────────
    auto final_attachment = vuk::declare_ia(
        "final",
        { .extent = info.extent,
          .format = vuk::Format::eR16G16B16A16Sfloat,
          .sample_count = vuk::Samples::e1,
          .level_count = 1,
          .layer_count = 1 });
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia(
        "depth",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eDepthStencilAttachment,
          .extent = info.extent,
          .format = vuk::Format::eD32Sfloat,
          .sample_count = vuk::Samples::e1,
          .level_count = 1,
          .layer_count = 1 });
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::ClearDepth(1.0));

    //  ── SKY VIEW LUT ────────────────────────────────────────────────────
    auto sky_view_pass = vuk::make_pass(
        "sky view",
        [&pipeline = *self.device->pipeline(self.sky_view_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
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

            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, dst)
                .bind_image(0, 2, transmittance_lut)
                .bind_image(0, 3, multiscatter_lut)
                .bind_buffer(0, 4, scene)
                .dispatch(groups.x, groups.y, 1);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut, scene);
        });

    auto transmittance_lut_attachment = self.sky_transmittance_lut_view.acquire(
        *self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
    auto multiscatter_lut_attachment = self.sky_multiscatter_lut_view.acquire(
        *self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);

    auto sky_view_lut_attachment = vuk::declare_ia(
        "sky_view_lut",
        { .image_type = vuk::ImageType::e2D,
          .extent = self.sky_view_lut_extent,
          .sample_count = vuk::Samples::e1,
          .view_type = vuk::ImageViewType::e2D,
          .level_count = 1,
          .layer_count = 1 });
    sky_view_lut_attachment.same_format_as(final_attachment);

    if (info.render_atmos) {
        std::tie(sky_view_lut_attachment, transmittance_lut_attachment, multiscatter_lut_attachment, scene_buffer) = sky_view_pass(
            std::move(sky_view_lut_attachment),
            std::move(transmittance_lut_attachment),
            std::move(multiscatter_lut_attachment),
            std::move(scene_buffer));
    }

    //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
    auto sky_aerial_perspective_pass = vuk::make_pass(
        "sky aerial perspective",
        [&pipeline = *self.device->pipeline(self.sky_aerial_perspective_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
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

            auto image_size = glm::vec3(dst->extent.width, dst->extent.height, dst->extent.depth);
            auto groups = glm::uvec2(image_size) / 16_u32;
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, sampler_info)
                .bind_image(0, 1, dst)
                .bind_image(0, 2, transmittance_lut)
                .bind_image(0, 3, multiscatter_lut)
                .bind_buffer(0, 4, scene)
                .dispatch(groups.x, groups.y, static_cast<u32>(image_size.z));
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut, scene);
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

    if (info.render_atmos) {
        std::tie(sky_aerial_perspective_attachment, transmittance_lut_attachment, multiscatter_lut_attachment, scene_buffer) =
            sky_aerial_perspective_pass(
                std::move(sky_aerial_perspective_attachment),
                std::move(transmittance_lut_attachment),
                std::move(multiscatter_lut_attachment),
                std::move(scene_buffer));
    }

    //  ── VISIBILITY BUFFER ───────────────────────────────────────────────
    auto visbuffer_attachment = vuk::declare_ia(
        "visbuffer",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR32Uint,
          .sample_count = vuk::Samples::e1 });
    visbuffer_attachment.same_shape_as(final_attachment);
    visbuffer_attachment = vuk::clear_image(std::move(visbuffer_attachment), vuk::Clear(vuk::ClearColor(~0_u32, 0_u32, 0_u32, 0_u32)));

    auto albedo_attachment = vuk::declare_ia(
        "albedo",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR8G8B8A8Unorm,
          .sample_count = vuk::Samples::e1 });
    albedo_attachment.same_shape_as(visbuffer_attachment);
    albedo_attachment = vuk::clear_image(std::move(albedo_attachment), vuk::Black<f32>);

    auto normal_attachment = vuk::declare_ia(
        "normal",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eR16G16B16A16Sfloat,
          .sample_count = vuk::Samples::e1 });
    normal_attachment.same_shape_as(visbuffer_attachment);
    normal_attachment = vuk::clear_image(std::move(normal_attachment), vuk::Black<f32>);

    auto emissive_attachment = vuk::declare_ia(
        "emissive",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eB10G11R11UfloatPack32,
          .sample_count = vuk::Samples::e1 });
    emissive_attachment.same_shape_as(visbuffer_attachment);
    emissive_attachment = vuk::clear_image(std::move(emissive_attachment), vuk::Black<f32>);

    if (self.meshlet_instance_count) {
        //  ── CULL MESHLETS ───────────────────────────────────────────────────
        auto vis_cull_meshlets_pass = vuk::make_pass(
            "vis cull meshlets",
            [&pipeline = *self.device->pipeline(self.vis_cull_meshlets_pipeline.id()),
             meshlet_instance_count = self.meshlet_instance_count](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) scene,
                VUK_BA(vuk::eComputeWrite) meshlet_indirect,
                VUK_BA(vuk::eComputeWrite) visible_meshlet_instances_indices) {
                cmd_list  //
                    .bind_compute_pipeline(pipeline)
                    .bind_buffer(0, 0, scene)
                    .bind_buffer(0, 1, meshlet_indirect)
                    .bind_buffer(0, 2, visible_meshlet_instances_indices)
                    .dispatch((meshlet_instance_count + Model::MAX_MESHLET_INDICES - 1) / Model::MAX_MESHLET_INDICES);
                return std::make_tuple(scene, meshlet_indirect, visible_meshlet_instances_indices);
            });

        auto meshlets_indirect_dispatch = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
        auto visible_meshlet_instances_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * sizeof(u32));

        std::tie(scene_buffer, meshlets_indirect_dispatch, visible_meshlet_instances_buffer) = vis_cull_meshlets_pass(
            std::move(scene_buffer), std::move(meshlets_indirect_dispatch), std::move(visible_meshlet_instances_buffer));

        //  ── CULL TRIANGLES ──────────────────────────────────────────────────
        auto vis_cull_triangles_pass = vuk::make_pass(
            "vis cull triangles",
            [&pipeline = *self.device->pipeline(self.vis_cull_triangles_pipeline.id())](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeWrite) triangles_indirect,
                VUK_BA(vuk::eIndirectRead) meshlet_indirect,
                VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRead) models,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeWrite) reordered_indices) {
                cmd_list  //
                    .bind_compute_pipeline(pipeline)
                    .bind_buffer(0, 0, triangles_indirect)
                    .bind_buffer(0, 1, visible_meshlet_instances_indices)
                    .bind_buffer(0, 2, models)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, reordered_indices)
                    .dispatch_indirect(meshlet_indirect);
                return std::make_tuple(triangles_indirect, visible_meshlet_instances_indices, models, meshlet_instances, reordered_indices);
            });

        auto triangles_indexed_indirect_dispatch = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });
        auto reordered_indices_buffer = transfer_man.alloc_transient_buffer(
            vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * Model::MAX_MESHLET_PRIMITIVES * 3 * sizeof(u32));

        vuk::Value<vuk::Buffer> materials_buffer;
        vuk::Value<vuk::Buffer> meshes_buffer;
        vuk::Value<vuk::Buffer> meshlet_instances_buffer;
        if (composed_scene.has_value()) {
            materials_buffer = std::move(composed_scene->materials_buffer);
            meshes_buffer = std::move(composed_scene->meshes_buffer);
            meshlet_instances_buffer = std::move(composed_scene->meshlet_instances_buffer);
        } else {
            materials_buffer = self.materials_buffer.acquire(*self.device, "Materials", vuk::Access::eNone);
            meshes_buffer = self.meshes_buffer.acquire(*self.device, "Meshes", vuk::Access::eNone);
            meshlet_instances_buffer = self.meshlet_instances_buffer.acquire(*self.device, "Meshlet Instances", vuk::Access::eNone);
        }

        std::tie(
            triangles_indexed_indirect_dispatch,
            visible_meshlet_instances_buffer,
            meshes_buffer,
            meshlet_instances_buffer,
            reordered_indices_buffer) =
            vis_cull_triangles_pass(
                std::move(triangles_indexed_indirect_dispatch),
                std::move(meshlets_indirect_dispatch),
                std::move(visible_meshlet_instances_buffer),
                std::move(meshes_buffer),
                std::move(meshlet_instances_buffer),
                std::move(reordered_indices_buffer));

        //  ── VISBUFFER ENCODE ────────────────────────────────────────────────
        auto vis_encode_pass = vuk::make_pass(
            "vis encode",
            [&pipeline = *self.device->pipeline(self.vis_encode_pipeline.id())](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) dst,
                VUK_IA(vuk::eDepthStencilRW) dst_depth,
                VUK_BA(vuk::eIndirectRead) triangle_indirect,
                VUK_BA(vuk::eVertexRead) scene,
                VUK_BA(vuk::eVertexRead) transforms,
                VUK_BA(vuk::eVertexRead) models,
                VUK_BA(vuk::eVertexRead) meshlet_instances,
                VUK_BA(vuk::eVertexRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eIndexRead) index_buffer) {
                cmd_list  //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                    .set_depth_stencil(
                        { .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                    .set_color_blend(dst, vuk::BlendPreset::eOff)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, scene)
                    .bind_buffer(0, 1, transforms)
                    .bind_buffer(0, 2, models)
                    .bind_buffer(0, 3, meshlet_instances)
                    .bind_buffer(0, 4, visible_meshlet_instances_indices)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                    .draw_indexed_indirect(1, triangle_indirect);
                return std::make_tuple(dst, dst_depth, scene, transforms, models, meshlet_instances, visible_meshlet_instances_indices);
            });

        std::tie(
            visbuffer_attachment,
            depth_attachment,
            scene_buffer,
            transforms_buffer,
            meshes_buffer,
            meshlet_instances_buffer,
            visible_meshlet_instances_buffer) =
            vis_encode_pass(
                std::move(visbuffer_attachment),
                std::move(depth_attachment),
                std::move(triangles_indexed_indirect_dispatch),
                std::move(scene_buffer),
                std::move(transforms_buffer),
                std::move(meshes_buffer),
                std::move(meshlet_instances_buffer),
                std::move(visible_meshlet_instances_buffer),
                std::move(reordered_indices_buffer));

        //  ── VISBUFFER DECODE ────────────────────────────────────────────────
        auto vis_decode_pass = vuk::make_pass(
            "vis decode",
            [&pipeline = *self.device->pipeline(self.vis_decode_pipeline.id()), &descriptor_set = self.bindless_set](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) albedo,
                VUK_IA(vuk::eColorWrite) normal,
                VUK_IA(vuk::eColorWrite) emissive,
                VUK_IA(vuk::eFragmentRead) visbuffer,
                VUK_BA(vuk::eFragmentRead) scene,
                VUK_BA(vuk::eFragmentRead) transforms,
                VUK_BA(vuk::eFragmentRead) meshes,
                VUK_BA(vuk::eFragmentRead) meshlet_instances,
                VUK_BA(vuk::eFragmentRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eFragmentRead) materials) {
                cmd_list  //
                    .bind_graphics_pipeline(pipeline)
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                    .set_depth_stencil({})
                    .set_color_blend(albedo, vuk::BlendPreset::eOff)
                    .set_color_blend(normal, vuk::BlendPreset::eOff)
                    .set_color_blend(emissive, vuk::BlendPreset::eOff)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_image(0, 0, visbuffer)
                    .bind_buffer(0, 1, scene)
                    .bind_buffer(0, 2, transforms)
                    .bind_buffer(0, 3, meshes)
                    .bind_buffer(0, 4, meshlet_instances)
                    .bind_buffer(0, 5, visible_meshlet_instances_indices)
                    .bind_buffer(0, 6, materials)
                    .bind_persistent(1, descriptor_set)
                    .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, glm::vec2(visbuffer->extent.width, visbuffer->extent.height))
                    .draw(3, 1, 0, 1);
                return std::make_tuple(albedo, normal, emissive, scene, transforms);
            });

        std::tie(albedo_attachment, normal_attachment, emissive_attachment, scene_buffer, transforms_buffer) = vis_decode_pass(
            std::move(albedo_attachment),
            std::move(normal_attachment),
            std::move(emissive_attachment),
            std::move(visbuffer_attachment),
            std::move(scene_buffer),
            std::move(transforms_buffer),
            std::move(meshes_buffer),
            std::move(meshlet_instances_buffer),
            std::move(visible_meshlet_instances_buffer),
            std::move(materials_buffer));
    }

    //  ── PBR BASIC ───────────────────────────────────────────────────────
    auto pbr_basic_pass = vuk::make_pass(
        "pbr basic",
        [&pipeline = *self.device->pipeline(self.pbr_basic_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) albedo,
            VUK_IA(vuk::Access::eFragmentSampled) normal,
            VUK_IA(vuk::Access::eFragmentSampled) emissive) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, albedo)
                .bind_image(0, 2, normal)
                .bind_image(0, 3, emissive)
                .set_rasterization({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .draw(3, 1, 0, 0);
            return dst;
        });

    final_attachment = pbr_basic_pass(
        std::move(final_attachment), std::move(albedo_attachment), std::move(normal_attachment), std::move(emissive_attachment));

    //  ── SKY FINAL ───────────────────────────────────────────────────────
    if (info.render_atmos) {
        auto sky_final_pass = vuk::make_pass(
            "sky final",
            [&pipeline = *self.device->pipeline(self.sky_final_pipeline.id())](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eColorWrite) dst,
                VUK_IA(vuk::Access::eFragmentRead) depth,
                VUK_IA(vuk::Access::eFragmentSampled) sky_view_lut,
                VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
                VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
                VUK_BA(vuk::Access::eFragmentRead) scene) {
                vuk::SamplerCreateInfo sampler_info = {
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
                    .set_rasterization({})
                    .set_depth_stencil({})
                    .set_color_blend(dst, blend_info)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_image(0, 0, depth)
                    .bind_sampler(0, 1, sampler_info)
                    .bind_image(0, 2, transmittance_lut)
                    .bind_image(0, 3, sky_view_lut)
                    .bind_image(0, 4, aerial_perspective_lut)
                    .bind_buffer(0, 5, scene)
                    .draw(3, 1, 0, 0);
                return std::make_tuple(dst, depth, sky_view_lut, aerial_perspective_lut, transmittance_lut, scene);
            });

        std::tie(
            final_attachment,
            depth_attachment,
            sky_view_lut_attachment,
            sky_aerial_perspective_attachment,
            transmittance_lut_attachment,
            scene_buffer) =
            sky_final_pass(
                std::move(final_attachment),
                std::move(depth_attachment),
                std::move(sky_view_lut_attachment),
                std::move(sky_aerial_perspective_attachment),
                std::move(transmittance_lut_attachment),
                std::move(scene_buffer));
    }

    if (info.render_clouds) {
        //  ── CLOUDS ──────────────────────────────────────────────────────────
        auto cloud_shape_noise_lut_attachment = self.cloud_shape_noise_lut_view.acquire(
            *self.device, "cloud shape lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
        auto cloud_detail_noise_lut_attachment = self.cloud_detail_noise_lut_view.acquire(
            *self.device, "cloud detail lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

        auto cloud_apply_pass = vuk::make_pass(
            "cloud apply",
            [&pipeline = *self.device->pipeline(self.cloud_apply_pipeline.id())](
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::Access::eColorRW) dst,
                VUK_IA(vuk::Access::eFragmentRead) depth,
                VUK_IA(vuk::Access::eFragmentSampled) shape_noise_lut,
                VUK_IA(vuk::Access::eFragmentSampled) detail_noise_lut,
                VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
                VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
                VUK_BA(vuk::Access::eFragmentRead) scene) {
                vuk::SamplerCreateInfo linear_repeat_sampler = {
                    .magFilter = vuk::Filter::eLinear,
                    .minFilter = vuk::Filter::eLinear,
                    .addressModeU = vuk::SamplerAddressMode::eRepeat,
                    .addressModeV = vuk::SamplerAddressMode::eRepeat,
                    .addressModeW = vuk::SamplerAddressMode::eRepeat,
                };
                vuk::SamplerCreateInfo linear_clamp_to_edge_sampler = {
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
                    .set_rasterization({})
                    .set_color_blend(dst, blend_info)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_image(0, 0, depth)
                    .bind_sampler(0, 1, linear_repeat_sampler)
                    .bind_sampler(0, 2, linear_clamp_to_edge_sampler)
                    .bind_image(0, 3, shape_noise_lut)
                    .bind_image(0, 4, detail_noise_lut)
                    .bind_image(0, 5, transmittance_lut)
                    .bind_image(0, 6, aerial_perspective_lut)
                    .bind_buffer(0, 7, scene)
                    .draw(3, 1, 0, 0);
                return std::make_tuple(dst, depth, transmittance_lut, aerial_perspective_lut, scene);
            });

        std::tie(final_attachment, depth_attachment, transmittance_lut_attachment, sky_aerial_perspective_attachment, scene_buffer) =
            cloud_apply_pass(
                std::move(final_attachment),
                std::move(depth_attachment),
                std::move(cloud_shape_noise_lut_attachment),
                std::move(cloud_detail_noise_lut_attachment),
                std::move(transmittance_lut_attachment),
                std::move(sky_aerial_perspective_attachment),
                std::move(scene_buffer));
    }

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [&pipeline = *self.device->pipeline(self.grid_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eDepthStencilRW) depth,
            VUK_BA(vuk::Access::eVertexRead | vuk::Access::eFragmentRead) scene) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_buffer(0, 0, scene)
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth, scene);
        });

    std::tie(final_attachment, depth_attachment, scene_buffer) =
        editor_grid_pass(std::move(final_attachment), std::move(depth_attachment), std::move(scene_buffer));

    auto result_attachment =
        vuk::declare_ia("result", { .extent = info.extent, .format = info.format, .sample_count = vuk::Samples::e1, .layer_count = 1 });

    //  ── TONEMAP ─────────────────────────────────────────────────────────
    std::tie(result_attachment, final_attachment) = vuk::make_pass(
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
        })(std::move(result_attachment), std::move(final_attachment));

    return result_attachment;
}

}  // namespace lr
