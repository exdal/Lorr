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

auto SceneRenderer::destroy(this SceneRenderer &self) -> void {
    ZoneScoped;

    self.cleanup();
}

auto SceneRenderer::create_persistent_resources(this SceneRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &transfer_man = app.device.transfer_man();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    constexpr auto MATERIAL_COUNT = 1024_sz;
    BindlessDescriptorInfo bindless_set_info[] = {
        { .binding = 0, .type = vuk::DescriptorType::eSampler, .descriptor_count = MATERIAL_COUNT },
        { .binding = 1, .type = vuk::DescriptorType::eSampledImage, .descriptor_count = MATERIAL_COUNT },
    };
    self.materials_descriptor_set = self.device->create_persistent_descriptor_set(bindless_set_info, 1).release();
    auto invalid_image_info = ImageInfo{
        .format = vuk::Format::eR8G8B8A8Srgb,
        .usage = vuk::ImageUsageFlagBits::eSampled,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 1, .height = 1, .depth = 1 },
        .name = "Invalid",
    };
    std::tie(self.invalid_image, self.invalid_image_view) = Image::create_with_view(*self.device, invalid_image_info).value();
    auto invalid_image = self.device->image_view(self.invalid_image_view.id());

    auto full_white = 0xFFFFFFFF_u32;
    transfer_man.wait_on(transfer_man.upload_staging(self.invalid_image_view, &full_white, sizeof(decltype(full_white))));

    auto invalid_sampler_info = SamplerInfo{
        .min_filter = vuk::Filter::eLinear,
        .mag_filter = vuk::Filter::eLinear,
        .mipmap_mode = vuk::SamplerMipmapMode::eLinear,
        .addr_u = vuk::SamplerAddressMode::eRepeat,
        .addr_v = vuk::SamplerAddressMode::eRepeat,
        .addr_w = vuk::SamplerAddressMode::eRepeat,
        .compare_op = vuk::CompareOp::eNever,
    };
    auto invalid_sampler = Sampler::create(*self.device, invalid_sampler_info).value();
    auto invalid_sampler_handle = self.device->sampler(invalid_sampler.id());

    for (auto i = 0_sz; i < MATERIAL_COUNT; i++) {
        self.materials_descriptor_set.update_sampler(0, i, *invalid_sampler_handle);
        self.materials_descriptor_set.update_sampled_image(1, i, *invalid_image, vuk::ImageLayout::eShaderReadOnlyOptimal);
    }
    self.device->commit_descriptor_set(self.materials_descriptor_set);
    self.device->destroy(invalid_sampler.id());

    //  ── EDITOR ──────────────────────────────────────────────────────────
    auto default_slang_session = self.device->new_slang_session({
        .definitions = {
#ifdef LS_DEBUG
            { "ENABLE_ASSERTIONS", "1" },
#endif // DEBUG
            { "CULLING_MESH_COUNT", "64" },
            { "CULLING_MESHLET_COUNT", std::to_string(Model::MAX_MESHLET_INDICES) },
            { "CULLING_TRIANGLE_COUNT", std::to_string(Model::MAX_MESHLET_PRIMITIVES) },
            { "MESH_MAX_LODS", std::to_string(GPU::Mesh::MAX_LODS) },
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
    auto vis_cull_meshes_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.cull_meshes",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_cull_meshes_pipeline_info).value();

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
    Pipeline::create(*self.device, default_slang_session, vis_encode_pipeline_info, self.materials_descriptor_set).value();

    auto vis_clear_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_clear",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_clear_pipeline_info).value();

    auto vis_decode_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_decode",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, vis_decode_pipeline_info, self.materials_descriptor_set).value();

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

    auto copy_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.copy",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, copy_pipeline_info).value();

    //  ── FFX ─────────────────────────────────────────────────────────────
    auto hiz_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.hiz",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(*self.device, default_slang_session, hiz_pipeline_info).value();

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
                .bind_buffer(0, 1, atmos)
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
                .bind_buffer(0, 2, atmos)
                .bind_image(0, 3, sky_multiscatter_lut)
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
    vuk::fill(vuk::acquire_buf("exposure", *self.device->buffer(self.exposure_buffer.id()), vuk::eNone), 0);
}

auto SceneRenderer::prepare_frame(this SceneRenderer &self, FramePrepareInfo &info) -> PreparedFrame {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();
    auto prepared_frame = PreparedFrame{};

    if (!info.dirty_transform_ids.empty()) {
        auto rebuild_transforms = !self.materials_buffer || self.transforms_buffer.data_size() <= info.gpu_transforms.size_bytes();
        self.transforms_buffer = self.transforms_buffer.resize(*self.device, info.gpu_transforms.size_bytes()).value();

        if (rebuild_transforms) {
            // If we resize buffer, we need to refill it again, so individual uploads are not required.
            prepared_frame.transforms_buffer = transfer_man.upload_staging(info.gpu_transforms, self.transforms_buffer);
        } else {
            // Buffer is not resized, upload individual transforms.

            auto dirty_transforms_count = info.dirty_transform_ids.size();
            auto dirty_transforms_size_bytes = dirty_transforms_count * sizeof(GPU::Transforms);
            auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUtoGPU, dirty_transforms_size_bytes);
            auto *dst_transform_ptr = reinterpret_cast<GPU::Transforms *>(upload_buffer->mapped_ptr);
            auto upload_offsets = std::vector<u64>(dirty_transforms_count);

            for (const auto &[dirty_transform_id, offset] : std::views::zip(info.dirty_transform_ids, upload_offsets)) {
                auto index = SlotMap_decode_id(dirty_transform_id).index;
                const auto &transform = info.gpu_transforms[index];
                std::memcpy(dst_transform_ptr, &transform, sizeof(GPU::Transforms));
                offset = index * sizeof(GPU::Transforms);
                dst_transform_ptr++;
            }

            auto update_transforms_pass = vuk::make_pass(
                "update scene transforms",
                [upload_offsets = std::move(upload_offsets)](
                    vuk::CommandBuffer &cmd_list, //
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

            prepared_frame.transforms_buffer = self.transforms_buffer.acquire(*self.device, "transforms", vuk::Access::eMemoryRead);
            prepared_frame.transforms_buffer = update_transforms_pass(std::move(upload_buffer), std::move(prepared_frame.transforms_buffer));
        }
    } else if (self.transforms_buffer) {
        prepared_frame.transforms_buffer = self.transforms_buffer.acquire(*self.device, "transforms", vuk::Access::eMemoryRead);
    }

    if (!info.dirty_texture_indices.empty()) {
        for (const auto &[texture_pair, index] : std::views::zip(info.dirty_textures, info.dirty_texture_indices)) {
            auto image_view = self.device->image_view(texture_pair.n0);
            auto sampler = self.device->sampler(texture_pair.n1);
            self.materials_descriptor_set.update_sampler(0, index, sampler.value());
            self.materials_descriptor_set.update_sampled_image(1, index, image_view.value(), vuk::ImageLayout::eShaderReadOnlyOptimal);
        }

        self.device->commit_descriptor_set(self.materials_descriptor_set);
        self.device->wait(); // I have no idea how to enable UPDATE_AFTER_BIND in vuk
    }

    if (!info.dirty_material_indices.empty()) {
        auto rebuild_materials = !self.materials_buffer || self.materials_buffer.data_size() <= info.gpu_materials.size_bytes();
        self.materials_buffer = self.materials_buffer.resize(*self.device, info.gpu_materials.size_bytes()).value();

        if (rebuild_materials) {
            prepared_frame.materials_buffer = transfer_man.upload_staging(info.gpu_materials, self.materials_buffer);
        } else {
            // TODO: Literally repeating code, find a solution to this
            auto dirty_materials_count = info.dirty_material_indices.size();
            auto dirty_materials_size_bytes = dirty_materials_count * sizeof(GPU::Material);
            auto upload_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUtoGPU, dirty_materials_size_bytes);
            auto *dst_materials_ptr = reinterpret_cast<GPU::Material *>(upload_buffer->mapped_ptr);
            auto upload_offsets = std::vector<u32>(dirty_materials_count);

            for (const auto &[dirty_material, index, offset] : std::views::zip(info.gpu_materials, info.dirty_material_indices, upload_offsets)) {
                std::memcpy(dst_materials_ptr, &dirty_material, sizeof(GPU::Material));
                offset = index * sizeof(GPU::Material);
                dst_materials_ptr++;
            }

            auto update_materials_pass = vuk::make_pass(
                "update scene materials",
                [upload_offsets = std::move(upload_offsets)](
                    vuk::CommandBuffer &cmd_list, //
                    VUK_BA(vuk::Access::eTransferRead) src_buffer,
                    VUK_BA(vuk::Access::eTransferWrite) dst_buffer
                ) {
                    for (usize i = 0; i < upload_offsets.size(); i++) {
                        auto offset = upload_offsets[i];
                        auto src_subrange = src_buffer->subrange(i * sizeof(GPU::Material), sizeof(GPU::Material));
                        auto dst_subrange = dst_buffer->subrange(offset, sizeof(GPU::Material));
                        cmd_list.copy_buffer(src_subrange, dst_subrange);
                    }

                    return dst_buffer;
                }
            );

            prepared_frame.materials_buffer = self.materials_buffer.acquire(*self.device, "materials", vuk::eMemoryRead);
            prepared_frame.materials_buffer = update_materials_pass(std::move(upload_buffer), std::move(prepared_frame.materials_buffer));
        }
    } else if (self.materials_buffer) {
        prepared_frame.materials_buffer = self.materials_buffer.acquire(*self.device, "materials", vuk::eMemoryRead);
    }

    if (!info.gpu_meshes.empty()) {
        self.meshes_buffer = self.meshes_buffer.resize(*self.device, info.gpu_meshes.size_bytes()).value();
        prepared_frame.meshes_buffer = transfer_man.upload_staging(info.gpu_meshes, self.meshes_buffer);
    } else if (self.meshes_buffer) {
        prepared_frame.meshes_buffer = self.meshes_buffer.acquire(*self.device, "meshes", vuk::eMemoryRead);
    }

    if (!info.gpu_mesh_instances.empty()) {
        self.mesh_instances_buffer = self.mesh_instances_buffer.resize(*self.device, info.gpu_mesh_instances.size_bytes()).value();
        prepared_frame.mesh_instances_buffer = transfer_man.upload_staging(info.gpu_mesh_instances, self.mesh_instances_buffer);

        self.mesh_instance_count = info.gpu_mesh_instances.size();
    } else if (self.mesh_instances_buffer) {
        prepared_frame.mesh_instances_buffer = self.mesh_instances_buffer.acquire(*self.device, "mesh instances", vuk::eMemoryRead);
    }

    if (!info.gpu_meshlet_instances.empty()) {
        self.meshlet_instances_buffer = self.meshlet_instances_buffer.resize(*self.device, info.gpu_meshlet_instances.size_bytes()).value();
        prepared_frame.meshlet_instances_buffer = transfer_man.upload_staging(info.gpu_meshlet_instances, self.meshlet_instances_buffer);

        self.meshlet_instance_count = info.gpu_meshlet_instances.size();
    } else if (self.meshlet_instances_buffer) {
        prepared_frame.meshlet_instances_buffer = self.meshlet_instances_buffer.acquire(*self.device, "meshlet instances", vuk::eMemoryRead);
    }

    return prepared_frame;
}

auto SceneRenderer::render(this SceneRenderer &self, SceneRenderInfo &info, PreparedFrame &frame) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto &transfer_man = self.device->transfer_man();

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
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::DepthZero);

    auto sky_transmittance_lut_attachment =
        self.sky_transmittance_lut_view
            .acquire(*self.device, "sky transmittance lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
    auto sky_multiscatter_lut_attachment =
        self.sky_multiscatter_lut_view.acquire(*self.device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);

    auto hiz_extent = vuk::Extent3D{
        .width = (info.extent.width + 63_u32) & ~63_u32,
        .height = (info.extent.height + 63_u32) & ~63_u32,
        .depth = 1,
    };

    auto hiz_attachment = vuk::Value<vuk::ImageAttachment>{};
    if (self.hiz.extent() != hiz_extent || !self.hiz) {
        if (self.hiz_view) {
            self.device->destroy(self.hiz_view.id());
        }

        if (self.hiz) {
            self.device->destroy(self.hiz.id());
        }

        auto hiz_info = ImageInfo{
            .format = vuk::Format::eR32Sfloat,
            .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
            .type = vuk::ImageType::e2D,
            .extent = hiz_extent,
            .mip_count = std::bit_width(ls::max(hiz_extent.width, hiz_extent.height)) - 1_u32,
            .name = "HiZ",
        };
        self.hiz = Image::create(*self.device, hiz_info).value();

        auto hiz_view_info = ImageViewInfo{
            .image_usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
            .type = vuk::ImageViewType::e2D,
            .subresource_range = { .aspectMask = vuk::ImageAspectFlagBits::eColor, .levelCount = hiz_info.mip_count },
            .name = "HiZ View",
        };
        self.hiz_view = ImageView::create(*self.device, self.hiz, hiz_view_info).value();

        hiz_attachment =
            self.hiz_view.acquire(*self.device, "HiZ", vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage, vuk::eNone);
        hiz_attachment = vuk::clear_image(std::move(hiz_attachment), vuk::DepthZero);
    } else {
        hiz_attachment =
            self.hiz_view.acquire(*self.device, "HiZ", vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage, vuk::eComputeRW);
    }

    static constexpr auto sampler_min_clamp_reduction_mode = VkSamplerReductionModeCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
        .pNext = nullptr,
        .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
    };
    static constexpr auto hiz_sampler_info = vuk::SamplerCreateInfo{
        .pNext = &sampler_min_clamp_reduction_mode,
        .magFilter = vuk::Filter::eLinear,
        .minFilter = vuk::Filter::eLinear,
        .mipmapMode = vuk::SamplerMipmapMode::eNearest,
        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
    };
    //   ──────────────────────────────────────────────────────────────────────

    const auto debugging = self.debug_lines;
    auto debug_drawer_buffer = vuk::Value<vuk::Buffer>{};
    auto debug_draw_aabb_buffer = vuk::Value<vuk::Buffer>{};
    auto debug_draw_rect_buffer = vuk::Value<vuk::Buffer>{};
    if (debugging) {
        auto debug_aabb_data = GPU::DebugDrawData{
            .capacity = 1 << 16,
        };
        debug_draw_aabb_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, debug_aabb_data.capacity * sizeof(GPU::DebugAABB));

        auto debug_rect_data = GPU::DebugDrawData{
            .capacity = 1 << 16,
        };
        debug_draw_rect_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, debug_rect_data.capacity * sizeof(GPU::DebugRect));

        debug_drawer_buffer = transfer_man.scratch_buffer<GPU::DebugDrawer>({
            .aabb_draw_cmd = { .vertex_count = 24 },
            .aabb_data = debug_aabb_data,
            .aabb_buffer = debug_draw_aabb_buffer->device_address,
            .rect_draw_cmd = { .vertex_count = 8 },
            .rect_data = debug_rect_data,
            .rect_buffer = debug_draw_rect_buffer->device_address,

        });
    } else {
        debug_drawer_buffer = transfer_man.scratch_buffer<GPU::DebugDrawer>({});
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
        atmosphere_buffer = transfer_man.scratch_buffer(atmosphere);
    }

    auto camera_buffer = vuk::Value<vuk::Buffer>{};
    if (info.camera.has_value()) {
        camera_buffer = transfer_man.scratch_buffer(info.camera.value());
    }

    if (self.mesh_instance_count) {
        auto transforms_buffer = std::move(frame.transforms_buffer);
        auto meshes_buffer = std::move(frame.meshes_buffer);
        auto mesh_instances_buffer = std::move(frame.mesh_instances_buffer);
        auto meshlet_instances_buffer = std::move(frame.meshlet_instances_buffer);
        auto materials_buffer = std::move(frame.materials_buffer);

        //  ── CULL MESHES ─────────────────────────────────────────────────────
        auto vis_cull_meshes_pass = vuk::make_pass(
            "vis cull meshes",
            [mesh_instance_count = self.mesh_instance_count, cull_flags = info.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_BA(vuk::eComputeRead) mesh_instances,
                VUK_BA(vuk::eComputeRead) meshes,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRW) cull_meshlets_cmd,
                VUK_BA(vuk::eComputeWrite) visible_mesh_instances_indices,
                VUK_BA(vuk::eComputeRW) debug_drawer
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.cull_meshes")
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, mesh_instances)
                    .bind_buffer(0, 2, meshes)
                    .bind_buffer(0, 3, transforms)
                    .bind_buffer(0, 4, cull_meshlets_cmd)
                    .bind_buffer(0, 5, visible_mesh_instances_indices)
                    .bind_buffer(0, 6, debug_drawer)
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(mesh_instance_count, cull_flags))
                    .dispatch_invocations(mesh_instance_count);

                return std::make_tuple(camera, mesh_instances, meshes, transforms, cull_meshlets_cmd, visible_mesh_instances_indices, debug_drawer);
            }
        );

        auto cull_meshlets_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
        auto visible_mesh_instances_indices_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.mesh_instance_count * sizeof(u32));

        std::tie(
            camera_buffer,
            mesh_instances_buffer,
            meshes_buffer,
            transforms_buffer,
            cull_meshlets_cmd_buffer,
            visible_mesh_instances_indices_buffer,
            debug_drawer_buffer
        ) =
            vis_cull_meshes_pass(
                std::move(camera_buffer),
                std::move(mesh_instances_buffer),
                std::move(meshes_buffer),
                std::move(transforms_buffer),
                std::move(cull_meshlets_cmd_buffer),
                std::move(visible_mesh_instances_indices_buffer),
                std::move(debug_drawer_buffer)
            );

        //  ── CULL MESHLETS ───────────────────────────────────────────────────
        auto vis_cull_meshlets_pass = vuk::make_pass(
            "vis cull meshlets",
            [meshlet_instance_count = self.meshlet_instance_count, cull_flags = info.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eIndirectRead) dispatch_cmd,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) mesh_instances,
                VUK_BA(vuk::eComputeRead) meshes,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_IA(vuk::eComputeRead) hiz,
                VUK_BA(vuk::eComputeRW) cull_triangles_cmd,
                VUK_BA(vuk::eComputeWrite) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRW) debug_drawer
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.cull_meshlets")
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, meshlet_instances)
                    .bind_buffer(0, 2, mesh_instances)
                    .bind_buffer(0, 3, meshes)
                    .bind_buffer(0, 4, transforms)
                    .bind_image(0, 5, hiz)
                    .bind_sampler(0, 6, hiz_sampler_info)
                    .bind_buffer(0, 7, cull_triangles_cmd)
                    .bind_buffer(0, 8, visible_meshlet_instances_indices)
                    .bind_buffer(0, 9, debug_drawer)
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(meshlet_instance_count, cull_flags))
                    .dispatch_indirect(dispatch_cmd);

                return std::make_tuple(
                    camera,
                    meshlet_instances,
                    mesh_instances,
                    meshes,
                    transforms,
                    hiz,
                    cull_triangles_cmd,
                    visible_meshlet_instances_indices,
                    debug_drawer
                );
            }
        );

        auto cull_triangles_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
        auto visible_meshlet_instances_indices_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, self.meshlet_instance_count * sizeof(u32));

        std::tie(
            camera_buffer,
            meshlet_instances_buffer,
            mesh_instances_buffer,
            meshes_buffer,
            transforms_buffer,
            hiz_attachment,
            cull_triangles_cmd_buffer,
            visible_meshlet_instances_indices_buffer,
            debug_drawer_buffer
        ) =
            vis_cull_meshlets_pass(
                std::move(cull_meshlets_cmd_buffer),
                std::move(camera_buffer),
                std::move(meshlet_instances_buffer),
                std::move(mesh_instances_buffer),
                std::move(meshes_buffer),
                std::move(transforms_buffer),
                std::move(hiz_attachment),
                std::move(cull_triangles_cmd_buffer),
                std::move(visible_meshlet_instances_indices_buffer),
                std::move(debug_drawer_buffer)
            );

        //  ── CULL TRIANGLES ──────────────────────────────────────────────────
        auto vis_cull_triangles_pass = vuk::make_pass(
            "vis cull triangles",
            [cull_flags = info.cull_flags](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eIndirectRead) cull_triangles_cmd,
                VUK_BA(vuk::eComputeRead) camera,
                VUK_BA(vuk::eComputeRead) visible_meshlet_instances_indices,
                VUK_BA(vuk::eComputeRead) meshlet_instances,
                VUK_BA(vuk::eComputeRead) mesh_instances,
                VUK_BA(vuk::eComputeRead) meshes,
                VUK_BA(vuk::eComputeRead) transforms,
                VUK_BA(vuk::eComputeRW) draw_indexed_cmd,
                VUK_BA(vuk::eComputeWrite) reordered_indices
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.cull_triangles")
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, visible_meshlet_instances_indices)
                    .bind_buffer(0, 2, meshlet_instances)
                    .bind_buffer(0, 3, mesh_instances)
                    .bind_buffer(0, 4, meshes)
                    .bind_buffer(0, 5, transforms)
                    .bind_buffer(0, 6, draw_indexed_cmd)
                    .bind_buffer(0, 7, reordered_indices)
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, cull_flags)
                    .dispatch_indirect(cull_triangles_cmd);

                return std::make_tuple(
                    camera,
                    visible_meshlet_instances_indices,
                    meshlet_instances,
                    mesh_instances,
                    meshes,
                    transforms,
                    draw_indexed_cmd,
                    reordered_indices
                );
            }
        );

        auto draw_command_buffer = transfer_man.scratch_buffer<vuk::DrawIndexedIndirectCommand>({ .instanceCount = 1 });
        auto reordered_indices_buffer = transfer_man.alloc_transient_buffer(
            vuk::MemoryUsage::eGPUonly,
            self.meshlet_instance_count * Model::MAX_MESHLET_PRIMITIVES * 3 * sizeof(u32)
        );

        std::tie(
            camera_buffer,
            visible_meshlet_instances_indices_buffer,
            meshlet_instances_buffer,
            mesh_instances_buffer,
            meshes_buffer,
            transforms_buffer,
            draw_command_buffer,
            reordered_indices_buffer
        ) =
            vis_cull_triangles_pass(
                std::move(cull_triangles_cmd_buffer),
                std::move(camera_buffer),
                std::move(visible_meshlet_instances_indices_buffer),
                std::move(meshlet_instances_buffer),
                std::move(mesh_instances_buffer),
                std::move(meshes_buffer),
                std::move(transforms_buffer),
                std::move(draw_command_buffer),
                std::move(reordered_indices_buffer)
            );

        //  ── VISBUFFER CLEAR ─────────────────────────────────────────────────
        auto vis_clear_pass = vuk::make_pass(
            "vis clear",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eComputeWrite) visbuffer,
                VUK_IA(vuk::eComputeWrite) overdraw
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.visbuffer_clear")
                    .bind_image(0, 0, visbuffer)
                    .bind_image(0, 1, overdraw)
                    .push_constants(
                        vuk::ShaderStageFlagBits::eCompute,
                        0,
                        PushConstants(glm::uvec2(visbuffer->extent.width, visbuffer->extent.height))
                    )
                    .dispatch_invocations_per_pixel(visbuffer);

                return std::make_tuple(visbuffer, overdraw);
            }
        );

        auto visbuffer_attachment = vuk::declare_ia(
            "visbuffer",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR32Uint,
              .sample_count = vuk::Samples::e1 }
        );
        visbuffer_attachment.same_shape_as(final_attachment);

        auto overdraw_attachment = vuk::declare_ia(
            "overdraw",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR32Uint,
              .sample_count = vuk::Samples::e1 }
        );
        overdraw_attachment.same_shape_as(final_attachment);

        std::tie(visbuffer_attachment, overdraw_attachment) = vis_clear_pass(std::move(visbuffer_attachment), std::move(overdraw_attachment));

        //  ── VISBUFFER ENCODE ────────────────────────────────────────────────
        auto vis_encode_pass = vuk::make_pass(
            "vis encode",
            [descriptor_set = &self.materials_descriptor_set](
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eIndirectRead) triangle_indirect,
                VUK_BA(vuk::eIndexRead) index_buffer,
                VUK_BA(vuk::eVertexRead) camera,
                VUK_BA(vuk::eVertexRead) meshlet_instances,
                VUK_BA(vuk::eVertexRead) mesh_instances,
                VUK_BA(vuk::eVertexRead) meshes,
                VUK_BA(vuk::eVertexRead) transforms,
                VUK_BA(vuk::eFragmentRead) materials,
                VUK_IA(vuk::eColorRW) visbuffer,
                VUK_IA(vuk::eDepthStencilRW) depth,
                VUK_IA(vuk::eFragmentRW) overdraw
            ) {
                cmd_list //
                    .bind_graphics_pipeline("passes.visbuffer_encode")
                    .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eBack })
                    .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual })
                    .set_color_blend(visbuffer, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_persistent(1, *descriptor_set)
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, meshlet_instances)
                    .bind_buffer(0, 2, mesh_instances)
                    .bind_buffer(0, 3, meshes)
                    .bind_buffer(0, 4, transforms)
                    .bind_buffer(0, 5, materials)
                    .bind_image(0, 6, overdraw)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                    .draw_indexed_indirect(1, triangle_indirect);

                return std::make_tuple(camera, meshlet_instances, mesh_instances, meshes, transforms, materials, visbuffer, depth, overdraw);
            }
        );

        std::tie(
            camera_buffer,
            meshlet_instances_buffer,
            mesh_instances_buffer,
            meshes_buffer,
            transforms_buffer,
            materials_buffer,
            visbuffer_attachment,
            depth_attachment,
            overdraw_attachment
        ) =
            vis_encode_pass(
                std::move(draw_command_buffer),
                std::move(reordered_indices_buffer),
                std::move(camera_buffer),
                std::move(meshlet_instances_buffer),
                std::move(mesh_instances_buffer),
                std::move(meshes_buffer),
                std::move(transforms_buffer),
                std::move(materials_buffer),
                std::move(visbuffer_attachment),
                std::move(depth_attachment),
                std::move(overdraw_attachment)
            );

        //  ── EDITOR MOUSE PICKING ────────────────────────────────────────────
        if (info.picking_texel) {
            auto editor_mousepick_pass = vuk::make_pass(
                "editor mousepick",
                [picking_texel = *info.picking_texel](
                    vuk::CommandBuffer &cmd_list,
                    VUK_IA(vuk::eComputeSampled) visbuffer,
                    VUK_BA(vuk::eComputeRead) meshlet_instances,
                    VUK_BA(vuk::eComputeRead) mesh_instances,
                    VUK_BA(vuk::eComputeWrite) picked_transform_index_buffer
                ) {
                    cmd_list //
                        .bind_compute_pipeline("passes.editor_mousepick")
                        .bind_image(0, 0, visbuffer)
                        .bind_buffer(0, 1, meshlet_instances)
                        .bind_buffer(0, 2, mesh_instances)
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
            auto picked_texel = editor_mousepick_pass(visbuffer_attachment, meshlet_instances_buffer, mesh_instances_buffer, picking_texel_buffer);

            vuk::Compiler temp_compiler;
            picked_texel.wait(self.device->get_allocator(), temp_compiler);

            u32 texel_data = 0;
            std::memcpy(&texel_data, picked_texel->mapped_ptr, sizeof(u32));
            info.picked_transform_index = texel_data;
        }

        auto hiz_generate_pass = vuk::make_pass(
            "hiz generate",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eComputeSampled) src,
                VUK_IA(vuk::eComputeRW) dst
            ) {
                auto extent = dst->extent;
                auto mip_count = dst->level_count;
                LS_EXPECT(mip_count < 13);

                auto dispatch_x = (extent.width + 63) >> 6;
                auto dispatch_y = (extent.height + 63) >> 6;

                cmd_list //
                    .bind_compute_pipeline("passes.hiz")
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(mip_count, (dispatch_x * dispatch_y) - 1, glm::mat2(1.0f)))
                    .specialize_constants(0, extent.width == extent.height && (extent.width & (extent.width - 1)) == 0 ? 1u : 0u)
                    .specialize_constants(1, extent.width)
                    .specialize_constants(2, extent.height);

                *cmd_list.scratch_buffer<u32>(0, 0) = 0;
                cmd_list.bind_sampler(0, 1, hiz_sampler_info);
                cmd_list.bind_image(0, 2, src);

                for (u32 i = 0; i < 13; i++) {
                    cmd_list.bind_image(0, i + 3, dst->mip(ls::min(i, mip_count - 1_u32)));
                }

                cmd_list.dispatch(dispatch_x, dispatch_y);

                return std::make_tuple(src, dst);
            }
        );

        std::tie(depth_attachment, hiz_attachment) = hiz_generate_pass(std::move(depth_attachment), std::move(hiz_attachment));

        //  ── VISBUFFER DECODE ────────────────────────────────────────────────
        auto vis_decode_pass = vuk::make_pass(
            "vis decode",
            [descriptor_set = &self.materials_descriptor_set]( //
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eFragmentRead) camera,
                VUK_BA(vuk::eFragmentRead) meshlet_instances,
                VUK_BA(vuk::eFragmentRead) mesh_instances,
                VUK_BA(vuk::eFragmentRead) meshes,
                VUK_BA(vuk::eFragmentRead) transforms,
                VUK_BA(vuk::eFragmentRead) materials,
                VUK_IA(vuk::eFragmentRead) visbuffer,
                VUK_IA(vuk::eColorRW) albedo,
                VUK_IA(vuk::eColorRW) normal,
                VUK_IA(vuk::eColorRW) emissive,
                VUK_IA(vuk::eColorRW) metallic_roughness_occlusion
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
                    .bind_persistent(1, *descriptor_set)
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, meshlet_instances)
                    .bind_buffer(0, 2, mesh_instances)
                    .bind_buffer(0, 3, meshes)
                    .bind_buffer(0, 4, transforms)
                    .bind_buffer(0, 5, materials)
                    .bind_image(0, 6, visbuffer)
                    .draw(3, 1, 0, 1);

                return std::make_tuple(
                    camera,
                    meshlet_instances,
                    mesh_instances,
                    meshes,
                    transforms,
                    visbuffer,
                    albedo,
                    normal,
                    emissive,
                    metallic_roughness_occlusion
                );
            }
        );

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

        std::tie(
            camera_buffer,
            meshlet_instances_buffer,
            mesh_instances_buffer,
            meshes_buffer,
            transforms_buffer,
            visbuffer_attachment,
            albedo_attachment,
            normal_attachment,
            emissive_attachment,
            metallic_roughness_occlusion_attachment
        ) =
            vis_decode_pass(
                std::move(camera_buffer),
                std::move(meshlet_instances_buffer),
                std::move(mesh_instances_buffer),
                std::move(meshes_buffer),
                std::move(transforms_buffer),
                std::move(materials_buffer),
                std::move(visbuffer_attachment),
                std::move(albedo_attachment),
                std::move(normal_attachment),
                std::move(emissive_attachment),
                std::move(metallic_roughness_occlusion_attachment)
            );

        if (info.atmosphere.has_value()) {
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
                    VUK_IA(vuk::eFragmentSampled) depth,
                    VUK_IA(vuk::eFragmentSampled) albedo,
                    VUK_IA(vuk::eFragmentSampled) normal,
                    VUK_IA(vuk::eFragmentSampled) emissive,
                    VUK_IA(vuk::eFragmentSampled) metallic_roughness_occlusion
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
                        .bind_buffer(0, 9, atmosphere)
                        .bind_buffer(0, 10, sun)
                        .bind_buffer(0, 11, camera)
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
    }

    if (info.atmosphere.has_value()) {
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
                VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                VUK_BA(vuk::eComputeRead) atmosphere,
                VUK_BA(vuk::eComputeRead) sun,
                VUK_BA(vuk::eComputeRead) camera,
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
                    .bind_buffer(0, 3, atmosphere)
                    .bind_buffer(0, 4, sun)
                    .bind_buffer(0, 5, camera)
                    .bind_image(0, 6, sky_view_lut)
                    .dispatch_invocations_per_pixel(sky_view_lut);
                return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, atmosphere, sun, camera, sky_view_lut);
            }
        );
        std::tie(
            sky_transmittance_lut_attachment,
            sky_multiscatter_lut_attachment,
            atmosphere_buffer,
            sun_buffer,
            camera_buffer,
            sky_view_lut_attachment
        ) =
            sky_view_pass(
                std::move(sky_transmittance_lut_attachment),
                std::move(sky_multiscatter_lut_attachment),
                std::move(atmosphere_buffer),
                std::move(sun_buffer),
                std::move(camera_buffer),
                std::move(sky_view_lut_attachment)
            );

        //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
        auto sky_aerial_perspective_pass = vuk::make_pass(
            "sky aerial perspective",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
                VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
                VUK_BA(vuk::eComputeRead) atmosphere,
                VUK_BA(vuk::eComputeRead) sun,
                VUK_BA(vuk::eComputeRead) camera,
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
                    .bind_buffer(0, 3, atmosphere)
                    .bind_buffer(0, 4, sun)
                    .bind_buffer(0, 5, camera)
                    .bind_image(0, 6, sky_aerial_perspective_lut)
                    .dispatch_invocations_per_pixel(sky_aerial_perspective_lut);
                return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, atmosphere, sun, camera, sky_aerial_perspective_lut);
            }
        );

        std::tie(
            sky_transmittance_lut_attachment,
            sky_multiscatter_lut_attachment,
            atmosphere_buffer,
            sun_buffer,
            camera_buffer,
            sky_aerial_perspective_attachment
        ) =
            sky_aerial_perspective_pass(
                std::move(sky_transmittance_lut_attachment),
                std::move(sky_multiscatter_lut_attachment),
                std::move(atmosphere_buffer),
                std::move(sun_buffer),
                std::move(camera_buffer),
                std::move(sky_aerial_perspective_attachment)
            );

        //  ── SKY FINAL ───────────────────────────────────────────────────────
        auto sky_final_pass = vuk::make_pass(
            "sky final",
            []( //
                vuk::CommandBuffer &cmd_list,
                VUK_IA(vuk::eColorWrite) dst,
                VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
                VUK_IA(vuk::eFragmentSampled) sky_aerial_perspective_lut,
                VUK_IA(vuk::eFragmentSampled) sky_view_lut,
                VUK_IA(vuk::eFragmentSampled) depth,
                VUK_BA(vuk::eFragmentRead) atmosphere,
                VUK_BA(vuk::eFragmentRead) sun,
                VUK_BA(vuk::eFragmentRead) camera
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
                    .bind_buffer(0, 5, atmosphere)
                    .bind_buffer(0, 6, sun)
                    .bind_buffer(0, 7, camera)
                    .draw(3, 1, 0, 0);
                return std::make_tuple(dst, depth, camera);
            }
        );

        std::tie(final_attachment, depth_attachment, camera_buffer) = sky_final_pass(
            std::move(final_attachment),
            std::move(sky_transmittance_lut_attachment),
            std::move(sky_aerial_perspective_attachment),
            std::move(sky_view_lut_attachment),
            std::move(depth_attachment),
            std::move(atmosphere_buffer),
            std::move(sun_buffer),
            std::move(camera_buffer)
        );
    }

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
                            histogram_info.ISO_K
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
    //std::tie(result_attachment, depth_attachment) =
    //    editor_grid_pass(std::move(result_attachment), std::move(depth_attachment), std::move(camera_buffer));

    if (debugging) {
        auto debug_pass = vuk::make_pass(
            "debug pass",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eColorWrite) dst,
               VUK_BA(vuk::eIndirectRead) indirect_buffer,
               VUK_BA(vuk::eVertexRead) camera,
               VUK_BA(vuk::eVertexRead) debug_aabb_draws,
               VUK_BA(vuk::eVertexRead) debug_rect_draws) {
                cmd_list //
                    .bind_graphics_pipeline("passes.debug")
                    .set_rasterization({ .polygonMode = vuk::PolygonMode::eFill, .lineWidth = 1.8f })
                    .set_primitive_topology(vuk::PrimitiveTopology::eLineList)
                    .set_color_blend(dst, vuk::BlendPreset::eOff)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, camera)
                    .bind_buffer(0, 1, debug_aabb_draws)
                    .bind_buffer(0, 2, debug_rect_draws);

                auto indirect_aabb_slice = indirect_buffer->subrange(offsetof(GPU::DebugDrawer, aabb_draw_cmd), sizeof(GPU::DrawIndirectCommand));
                cmd_list.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, 0_u32);
                cmd_list.draw_indirect(1, indirect_aabb_slice);

                auto indirect_rect_slice = indirect_buffer->subrange(offsetof(GPU::DebugDrawer, rect_draw_cmd), sizeof(GPU::DrawIndirectCommand));
                cmd_list.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, 1_u32);
                cmd_list.draw_indirect(1, indirect_rect_slice);

                return dst;
            }
        );

        result_attachment = debug_pass(
            std::move(result_attachment),
            std::move(debug_drawer_buffer),
            std::move(camera_buffer),
            std::move(debug_draw_aabb_buffer),
            std::move(debug_draw_rect_buffer)
        );
    }

    return result_attachment;
}

auto SceneRenderer::cleanup(this SceneRenderer &self) -> void {
    ZoneScoped;

    self.device->wait();

    self.mesh_instance_count = 0;

    if (self.transforms_buffer) {
        self.device->destroy(self.transforms_buffer.id());
        self.transforms_buffer = {};
    }

    if (self.meshlet_instances_buffer) {
        self.device->destroy(self.meshlet_instances_buffer.id());
        self.meshlet_instances_buffer = {};
    }

    if (self.mesh_instances_buffer) {
        self.device->destroy(self.mesh_instances_buffer.id());
        self.mesh_instances_buffer = {};
    }

    if (self.meshes_buffer) {
        self.device->destroy(self.meshes_buffer.id());
        self.meshes_buffer = {};
    }

    if (self.materials_buffer) {
        self.device->destroy(self.materials_buffer.id());
        self.materials_buffer = {};
    }

    if (self.hiz_view) {
        self.device->destroy(self.hiz_view.id());
        self.hiz_view = {};
    }

    if (self.hiz) {
        self.device->destroy(self.hiz.id());
        self.hiz = {};
    }
}

} // namespace lr
