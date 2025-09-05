#include "Engine/Scene/SceneRenderer.hh"

#include "Engine/Asset/Asset.hh"

#include "Engine/Core/App.hh"

#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Memory/Stack.hh"

namespace lr {
enum BindlessDescriptorLayout : u32 {
    Samplers = 0,
    SampledImages = 1,
};

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

auto SceneRenderer::init(this SceneRenderer &self) -> bool {
    ZoneScoped;

    auto &device = App::mod<Device>();
    auto &bindless_descriptor_set = device.get_descriptor_set();
    auto &asset_man = App::mod<AssetManager>();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    //  ── EDITOR ──────────────────────────────────────────────────────────
    auto default_slang_session = device.new_slang_session({
        .definitions = {
#ifdef LS_DEBUG
            { "ENABLE_ASSERTIONS", "1" },
            { "DEBUG_DRAW", "1" },
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
    Pipeline::create(device, default_slang_session, editor_grid_pipeline_info).value();

    auto editor_mousepick_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.editor_mousepick",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, editor_mousepick_pipeline_info).value();
    //  ── SKY ─────────────────────────────────────────────────────────────
    auto sky_transmittance_lut_info = ImageInfo{
        .format = vuk::Format::eR16G16B16A16Sfloat,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 256, .height = 64, .depth = 1 },
        .name = "Sky Transmittance",
    };
    std::tie(self.sky_transmittance_lut, self.sky_transmittance_lut_view) = Image::create_with_view(device, sky_transmittance_lut_info).value();
    auto sky_transmittance_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_transmittance",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, sky_transmittance_pipeline_info).value();

    auto sky_multiscatter_lut_info = ImageInfo{
        .format = vuk::Format::eR16G16B16A16Sfloat,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        .type = vuk::ImageType::e2D,
        .extent = { .width = 32, .height = 32, .depth = 1 },
        .name = "Sky Multiscatter LUT",
    };
    std::tie(self.sky_multiscatter_lut, self.sky_multiscatter_lut_view) = Image::create_with_view(device, sky_multiscatter_lut_info).value();
    auto sky_multiscatter_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_multiscattering",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, sky_multiscatter_pipeline_info).value();

    auto sky_view_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_view",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, sky_view_pipeline_info).value();

    auto sky_aerial_perspective_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_aerial_perspective",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, sky_aerial_perspective_pipeline_info).value();

    auto sky_final_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.sky_final",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, sky_final_pipeline_info).value();

    //  ── VISBUFFER ───────────────────────────────────────────────────────
    auto generate_cull_commands_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.generate_cull_commands",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, generate_cull_commands_pipeline_info).value();

    auto vis_cull_meshes_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.cull_meshes",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vis_cull_meshes_pipeline_info).value();

    auto vis_cull_meshlets_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.cull_meshlets",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vis_cull_meshlets_pipeline_info).value();

    auto vis_cull_triangles_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.cull_triangles",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vis_cull_triangles_pipeline_info).value();

    auto vis_encode_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_encode",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, vis_encode_pipeline_info, bindless_descriptor_set).value();

    auto vis_clear_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_clear",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vis_clear_pipeline_info).value();

    auto vis_decode_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visbuffer_decode",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, vis_decode_pipeline_info, bindless_descriptor_set).value();

    //  ── PBR ─────────────────────────────────────────────────────────────
    auto pbr_basic_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.brdf",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, pbr_basic_pipeline_info).value();

    //  ── POST PROCESS ────────────────────────────────────────────────────
    auto histogram_generate_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.histogram_generate",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, histogram_generate_pipeline_info).value();

    auto histogram_average_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.histogram_average",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, histogram_average_pipeline_info).value();

    auto tonemap_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.tonemap",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, tonemap_pipeline_info).value();

    auto debug_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.debug",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, debug_pipeline_info).value();

    auto copy_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.copy",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, copy_pipeline_info).value();

    auto hiz_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.hiz",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, hiz_pipeline_info).value();

    auto hiz_slow_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.hiz_slow",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, hiz_slow_pipeline_info).value();

    auto visualize_overdraw_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.visualize_overdraw",
        .entry_points = { "vs_main", "fs_main" },
    };
    Pipeline::create(device, default_slang_session, visualize_overdraw_pipeline_info).value();

    auto vbgtao_prefilter_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.vbgtao_prefilter",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vbgtao_prefilter_pipeline_info).value();

    auto vbgtao_generate_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.vbgtao_generate",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vbgtao_generate_pipeline_info).value();

    auto vbgtao_denoise_pipeline_info = PipelineCompileInfo{
        .module_name = "passes.vbgtao_denoise",
        .entry_points = { "cs_main" },
    };
    Pipeline::create(device, default_slang_session, vbgtao_denoise_pipeline_info).value();

    self.histogram_luminance_buffer = Buffer::create(device, sizeof(GPU::HistogramLuminance)).value();
    vuk::fill(vuk::acquire_buf("histogram luminance", *device.buffer(self.histogram_luminance_buffer.id()), vuk::eNone), 0);

    // Hilbert Noise LUT
    constexpr auto HILBERT_NOISE_LUT_WIDTH = 64_u32;
    auto hilbert_noise_lut_info = ImageInfo{
        .format = vuk::Format::eR16Uint,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eTransferDst,
        .type = vuk::ImageType::e2D,
        .extent = { .width = HILBERT_NOISE_LUT_WIDTH, .height = HILBERT_NOISE_LUT_WIDTH, .depth = 1 },
        .name = "Hilbert Noise LUT",
    };
    std::tie(self.hilbert_noise_lut, self.hilbert_noise_lut_view) = Image::create_with_view(device, hilbert_noise_lut_info).value();

    auto hilbert_index = [](u32 pos_x, u32 pos_y) -> u16 {
        auto index = 0_u32;
        for (auto cur_level = HILBERT_NOISE_LUT_WIDTH / 2; cur_level > 0_u32; cur_level /= 2_u32) {
            auto region_x = (pos_x & cur_level) > 0_u32;
            auto region_y = (pos_y & cur_level) > 0_u32;
            index += cur_level * cur_level * ((3_u32 * region_x) ^ region_y);
            if (region_y == 0_u32) {
                if (region_x == 1_u32) {
                    pos_x = (HILBERT_NOISE_LUT_WIDTH - 1_u32) - pos_x;
                    pos_y = (HILBERT_NOISE_LUT_WIDTH - 1_u32) - pos_y;
                }

                auto temp_pos_x = pos_x;
                pos_x = pos_y;
                pos_y = temp_pos_x;
            }
        }

        return index;
    };

    u16 hilbert_noise[HILBERT_NOISE_LUT_WIDTH * HILBERT_NOISE_LUT_WIDTH] = {};
    for (auto y = 0_u32; y < HILBERT_NOISE_LUT_WIDTH; y++) {
        for (auto x = 0_u32; x < HILBERT_NOISE_LUT_WIDTH; x++) {
            hilbert_noise[y * HILBERT_NOISE_LUT_WIDTH + x] = hilbert_index(x, y);
        }
    }

    auto &transfer_man = device.transfer_man();

    auto hilbert_noise_size_bytes = HILBERT_NOISE_LUT_WIDTH * HILBERT_NOISE_LUT_WIDTH * sizeof(u16);
    auto hilbert_noise_buffer = transfer_man.alloc_image_buffer(hilbert_noise_lut_info.format, hilbert_noise_lut_info.extent);
    std::memcpy(hilbert_noise_buffer->mapped_ptr, hilbert_noise, hilbert_noise_size_bytes);

    auto hilbert_noise_lut_attachment = self.hilbert_noise_lut_view.discard(device, "hilbert noise", vuk::ImageUsageFlagBits::eTransferDst);
    hilbert_noise_lut_attachment = transfer_man.upload(std::move(hilbert_noise_buffer), std::move(hilbert_noise_lut_attachment));
    hilbert_noise_lut_attachment = hilbert_noise_lut_attachment.as_released(vuk::eComputeSampled, vuk::DomainFlagBits::eGraphicsQueue);
    transfer_man.wait_on(std::move(hilbert_noise_lut_attachment));

    return true;
}

auto SceneRenderer::destroy(this SceneRenderer &self) -> void {
    ZoneScoped;

    self.cleanup();
}

auto SceneRenderer::prepare_frame(this SceneRenderer &self, FramePrepareInfo &info) -> PreparedFrame {
    ZoneScoped;

    auto &device = App::mod<Device>();
    auto &transfer_man = device.transfer_man();
    auto prepared_frame = PreparedFrame{};

    auto zero_fill_pass = vuk::make_pass("zero fill", [](vuk::CommandBuffer &command_buffer, VUK_BA(vuk::eTransferWrite) dst) {
        command_buffer.fill_buffer(dst, 0_u32);

        return dst;
    });

    if (!info.dirty_transform_ids.empty()) {
        auto rebuild_transforms = !self.transforms_buffer || self.transforms_buffer.data_size() <= info.gpu_transforms.size_bytes();
        self.transforms_buffer = self.transforms_buffer.resize(device, info.gpu_transforms.size_bytes()).value();
        prepared_frame.transforms_buffer = self.transforms_buffer.acquire(device, "transforms", rebuild_transforms ? vuk::eNone : vuk::eMemoryRead);

        if (rebuild_transforms) {
            // If we resize buffer, we need to refill it again, so individual uploads are not required.
            prepared_frame.transforms_buffer = transfer_man.upload(info.gpu_transforms, std::move(prepared_frame.transforms_buffer));
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

            prepared_frame.transforms_buffer = update_transforms_pass(std::move(upload_buffer), std::move(prepared_frame.transforms_buffer));
        }
    } else if (self.transforms_buffer) {
        prepared_frame.transforms_buffer = self.transforms_buffer.acquire(device, "transforms", vuk::Access::eMemoryRead);
    }

    if (!info.dirty_material_indices.empty()) {
        auto rebuild_materials = !self.materials_buffer || self.materials_buffer.data_size() <= info.gpu_materials.size_bytes();
        self.materials_buffer = self.materials_buffer.resize(device, info.gpu_materials.size_bytes()).value();
        prepared_frame.materials_buffer = self.materials_buffer.acquire(device, "materials", rebuild_materials ? vuk::eNone : vuk::eMemoryRead);

        if (rebuild_materials) {
            prepared_frame.materials_buffer = transfer_man.upload(info.gpu_materials, std::move(prepared_frame.materials_buffer));
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

            prepared_frame.materials_buffer = update_materials_pass(std::move(upload_buffer), std::move(prepared_frame.materials_buffer));
        }
    } else if (self.materials_buffer) {
        prepared_frame.materials_buffer = self.materials_buffer.acquire(device, "materials", vuk::eMemoryRead);
    }

    if (!info.gpu_meshes.empty()) {
        self.meshes_buffer = self.meshes_buffer.resize(device, info.gpu_meshes.size_bytes()).value();
        prepared_frame.meshes_buffer = self.meshes_buffer.acquire(device, "meshes", vuk::eNone);
        prepared_frame.meshes_buffer = transfer_man.upload(info.gpu_meshes, std::move(prepared_frame.meshes_buffer));
    } else if (self.meshes_buffer) {
        prepared_frame.meshes_buffer = self.meshes_buffer.acquire(device, "meshes", vuk::eMemoryRead);
    }

    if (!info.gpu_mesh_instances.empty()) {
        self.mesh_instances_buffer = self.mesh_instances_buffer.resize(device, info.gpu_mesh_instances.size_bytes()).value();
        prepared_frame.mesh_instances_buffer = self.mesh_instances_buffer.acquire(device, "mesh instances", vuk::eNone);
        prepared_frame.mesh_instances_buffer = transfer_man.upload(info.gpu_mesh_instances, std::move(prepared_frame.mesh_instances_buffer));

        auto meshlet_instance_visibility_mask_size_bytes = (info.max_meshlet_instance_count + 31) / 32 * sizeof(u32);
        self.meshlet_instance_visibility_mask_buffer =
            self.meshlet_instance_visibility_mask_buffer.resize(device, meshlet_instance_visibility_mask_size_bytes).value();
        prepared_frame.meshlet_instance_visibility_mask_buffer =
            self.meshlet_instance_visibility_mask_buffer.acquire(device, "meshlet instances visibility mask", vuk::eNone);
        prepared_frame.meshlet_instance_visibility_mask_buffer = zero_fill_pass(std::move(prepared_frame.meshlet_instance_visibility_mask_buffer));
    } else if (self.mesh_instances_buffer) {
        prepared_frame.mesh_instances_buffer = self.mesh_instances_buffer.acquire(device, "mesh instances", vuk::eMemoryRead);
        prepared_frame.meshlet_instance_visibility_mask_buffer =
            self.meshlet_instance_visibility_mask_buffer.acquire(device, "meshlet instances visibility mask", vuk::eMemoryRead);
    }

    info.environment.transmittance_lut_size = self.sky_transmittance_lut_view.extent();
    info.environment.sky_view_lut_size = self.sky_view_lut_extent;
    info.environment.multiscattering_lut_size = self.sky_multiscatter_lut_view.extent();
    info.environment.aerial_perspective_lut_size = self.sky_aerial_perspective_lut_extent;
    prepared_frame.environment_buffer = transfer_man.scratch_buffer(info.environment);

    // glm::vec3 corners[8] = {};
    // glm::vec3 ndc_corners[8] = {
    //     glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(-1.0f, -1.0f, 0.0f),
    //     glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, 1.0f),
    // };
    //
    // for (int i = 0; i < 8; ++i) {
    //     auto world_corner = info.camera.inv_projection_view_mat * glm::vec4(ndc_corners[i], 1.0f);
    //     corners[i] = glm::vec3(world_corner) / world_corner.w;
    // }
    //
    // auto center = glm::vec3(0.0f);
    // for (const auto &c : corners) {
    //     center += c;
    // }
    // center /= static_cast<f32>(ls::count_of(corners));
    //
    // auto shadow_map_size = 512.0f;
    // auto light_pos = center - info.environment.sun_direction * (shadow_map_size * 0.5f);
    // auto light_target = center;
    // auto up = glm::vec3(0, 1, 0);
    // if (1.0f - glm::abs(glm::dot(info.environment.sun_direction, up)) < 1e-4f) {
    //     up = glm::vec3(0, 0, 1);
    // }
    //
    // auto view_mat = glm::lookAt(light_pos, glm::vec3(0), up);
    // auto projection_mat = glm::orthoRH_ZO(
    //     -shadow_map_size * 0.5f,
    //     shadow_map_size * 0.5f,
    //     -shadow_map_size * 0.5f,
    //     shadow_map_size * 0.5f,
    //     -shadow_map_size * 0.5f,
    //     shadow_map_size * 0.5f
    // );
    // projection_mat[1][1] *= -1.0;

    prepared_frame.camera_buffer = transfer_man.scratch_buffer(info.camera);

    prepared_frame.mesh_instance_count = info.mesh_instance_count;
    prepared_frame.max_meshlet_instance_count = info.max_meshlet_instance_count;
    prepared_frame.environment_flags = static_cast<GPU::EnvironmentFlags>(info.environment.flags);

    if (info.regenerate_sky || !self.sky_transmittance_lut_view) {
        auto transmittance_lut_pass = vuk::make_pass(
            "transmittance lut",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eComputeRW) dst,
               VUK_BA(vuk::eComputeRead) environment) {
                cmd_list //
                    .bind_compute_pipeline("passes.sky_transmittance")
                    .bind_image(0, 0, dst)
                    .bind_buffer(0, 1, environment)
                    .dispatch_invocations_per_pixel(dst);

                return std::make_tuple(dst, environment);
            }
        );

        prepared_frame.sky_transmittance_lut =
            self.sky_transmittance_lut_view.discard(device, "sky transmittance lut", vuk::ImageUsageFlagBits::eStorage);
        std::tie(prepared_frame.sky_transmittance_lut, prepared_frame.environment_buffer) =
            transmittance_lut_pass(std::move(prepared_frame.sky_transmittance_lut), std::move(prepared_frame.environment_buffer));

        auto multiscatter_lut_pass = vuk::make_pass(
            "multiscatter lut",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
               VUK_IA(vuk::eComputeRW) sky_multiscatter_lut,
               VUK_BA(vuk::eComputeRead) environment) {
                cmd_list //
                    .bind_compute_pipeline("passes.sky_multiscattering")
                    .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                    .bind_image(0, 1, sky_transmittance_lut)
                    .bind_buffer(0, 2, environment)
                    .bind_image(0, 3, sky_multiscatter_lut)
                    .dispatch_invocations_per_pixel(sky_multiscatter_lut);

                return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, environment);
            }
        );

        prepared_frame.sky_multiscatter_lut =
            self.sky_multiscatter_lut_view.discard(device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eStorage);
        std::tie(prepared_frame.sky_transmittance_lut, prepared_frame.sky_multiscatter_lut, prepared_frame.environment_buffer) =
            multiscatter_lut_pass(
                std::move(prepared_frame.sky_transmittance_lut),
                std::move(prepared_frame.sky_multiscatter_lut),
                std::move(prepared_frame.environment_buffer)
            );
    } else {
        prepared_frame.sky_transmittance_lut =
            self.sky_transmittance_lut_view.acquire(device, "sky transmittance lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
        prepared_frame.sky_multiscatter_lut =
            self.sky_multiscatter_lut_view.acquire(device, "sky multiscatter lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eComputeSampled);
    }

    return prepared_frame;
}

static auto cull_meshes(
    GPU::CullFlags cull_flags,
    u32 mesh_instance_count,
    TransferManager &transfer_man,
    vuk::Value<vuk::Buffer> &meshes_buffer,
    vuk::Value<vuk::Buffer> &mesh_instances_buffer,
    vuk::Value<vuk::Buffer> &meshlet_instances_buffer,
    vuk::Value<vuk::Buffer> &visible_meshlet_instances_count_buffer,
    vuk::Value<vuk::Buffer> &transforms_buffer,
    vuk::Value<vuk::ImageAttachment> &hiz_attachment,
    vuk::Value<vuk::Buffer> &camera_buffer,
    vuk::Value<vuk::Buffer> &debug_drawer_buffer
) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;

    auto vis_cull_meshes_pass = vuk::make_pass(
        "vis cull meshes",
        [cull_flags, mesh_instance_count](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eComputeRead) camera,
            VUK_BA(vuk::eComputeRead) meshes,
            VUK_BA(vuk::eComputeRead) transforms,
            VUK_IA(vuk::eComputeSampled) hiz,
            VUK_BA(vuk::eComputeRW) mesh_instances,
            VUK_BA(vuk::eComputeRW) meshlet_instances,
            VUK_BA(vuk::eComputeRW) visible_meshlet_instances_count,
            VUK_BA(vuk::eComputeRW) debug_drawer
        ) {
            cmd_list //
                .bind_compute_pipeline("passes.cull_meshes")
                .bind_buffer(0, 0, camera)
                .bind_buffer(0, 1, meshes)
                .bind_buffer(0, 2, transforms)
                .bind_image(0, 3, hiz)
                .bind_sampler(0, 4, hiz_sampler_info)
                .bind_buffer(0, 5, mesh_instances)
                .bind_buffer(0, 6, meshlet_instances)
                .bind_buffer(0, 7, visible_meshlet_instances_count)
                .bind_buffer(0, 8, debug_drawer)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(mesh_instance_count, cull_flags))
                .dispatch_invocations(mesh_instance_count);

            return std::make_tuple(camera, meshes, transforms, hiz, mesh_instances, meshlet_instances, visible_meshlet_instances_count, debug_drawer);
        }
    );

    std::tie(
        camera_buffer,
        meshes_buffer,
        transforms_buffer,
        hiz_attachment,
        mesh_instances_buffer,
        meshlet_instances_buffer,
        visible_meshlet_instances_count_buffer,
        debug_drawer_buffer
    ) =
        vis_cull_meshes_pass(
            std::move(camera_buffer),
            std::move(meshes_buffer),
            std::move(transforms_buffer),
            std::move(hiz_attachment),
            std::move(mesh_instances_buffer),
            std::move(meshlet_instances_buffer),
            std::move(visible_meshlet_instances_count_buffer),
            std::move(debug_drawer_buffer)
        );

    auto generate_cull_commands_pass = vuk::make_pass(
        "generate cull commands",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_BA(vuk::eComputeRead) visible_meshlet_instances_count,
           VUK_BA(vuk::eComputeRW) cull_meshlets_cmd) {
            cmd_list //
                .bind_compute_pipeline("passes.generate_cull_commands")
                .bind_buffer(0, 0, visible_meshlet_instances_count)
                .bind_buffer(0, 1, cull_meshlets_cmd)
                .dispatch(1);

            return std::make_tuple(visible_meshlet_instances_count, cull_meshlets_cmd);
        }
    );

    auto cull_meshlets_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });
    std::tie(visible_meshlet_instances_count_buffer, cull_meshlets_cmd_buffer) =
        generate_cull_commands_pass(std::move(visible_meshlet_instances_count_buffer), std::move(cull_meshlets_cmd_buffer));

    return cull_meshlets_cmd_buffer;
}

static auto cull_meshlets(
    bool late,
    GPU::CullFlags cull_flags,
    TransferManager &transfer_man,
    vuk::Value<vuk::ImageAttachment> &hiz_attachment,
    vuk::Value<vuk::Buffer> &cull_meshlets_cmd_buffer,
    vuk::Value<vuk::Buffer> &visible_meshlet_instances_count_buffer,
    vuk::Value<vuk::Buffer> &visible_meshlet_instances_indices_buffer,
    vuk::Value<vuk::Buffer> &meshlet_instance_visibility_mask_buffer,
    vuk::Value<vuk::Buffer> &reordered_indices_buffer,
    vuk::Value<vuk::Buffer> &meshes_buffer,
    vuk::Value<vuk::Buffer> &mesh_instances_buffer,
    vuk::Value<vuk::Buffer> &meshlet_instances_buffer,
    vuk::Value<vuk::Buffer> &transforms_buffer,
    vuk::Value<vuk::Buffer> &camera_buffer,
    vuk::Value<vuk::Buffer> &debug_drawer_buffer
) -> vuk::Value<vuk::Buffer> {
    ZoneScoped;
    memory::ScopedStack stack;

    //  ── CULL MESHLETS ───────────────────────────────────────────────────
    auto vis_cull_meshlets_pass = vuk::make_pass(
        stack.format("vis cull meshlets {}", late ? "late" : "early"),
        [late, cull_flags](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eIndirectRead) dispatch_cmd,
            VUK_BA(vuk::eComputeRead) camera,
            VUK_BA(vuk::eComputeRead) meshlet_instances,
            VUK_BA(vuk::eComputeRead) mesh_instances,
            VUK_BA(vuk::eComputeRead) meshes,
            VUK_BA(vuk::eComputeRead) transforms,
            VUK_IA(vuk::eComputeSampled) hiz,
            VUK_BA(vuk::eComputeRW) visible_meshlet_instances_count,
            VUK_BA(vuk::eComputeRW) visible_meshlet_instances_indices,
            VUK_BA(vuk::eComputeRW) meshlet_instance_visibility_mask,
            VUK_BA(vuk::eComputeRW) cull_triangles_cmd,
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
                .bind_buffer(0, 7, visible_meshlet_instances_count)
                .bind_buffer(0, 8, visible_meshlet_instances_indices)
                .bind_buffer(0, 9, meshlet_instance_visibility_mask)
                .bind_buffer(0, 10, cull_triangles_cmd)
                .bind_buffer(0, 11, debug_drawer)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, cull_flags)
                .specialize_constants(0, late)
                .dispatch_indirect(dispatch_cmd);

            return std::make_tuple(
                dispatch_cmd,
                camera,
                meshlet_instances,
                mesh_instances,
                meshes,
                transforms,
                hiz,
                visible_meshlet_instances_count,
                visible_meshlet_instances_indices,
                meshlet_instance_visibility_mask,
                cull_triangles_cmd,
                debug_drawer
            );
        }
    );

    auto cull_triangles_cmd_buffer = transfer_man.scratch_buffer<vuk::DispatchIndirectCommand>({ .x = 0, .y = 1, .z = 1 });

    std::tie(
        cull_meshlets_cmd_buffer,
        camera_buffer,
        meshlet_instances_buffer,
        mesh_instances_buffer,
        meshes_buffer,
        transforms_buffer,
        hiz_attachment,
        visible_meshlet_instances_count_buffer,
        visible_meshlet_instances_indices_buffer,
        meshlet_instance_visibility_mask_buffer,
        cull_triangles_cmd_buffer,
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
            std::move(visible_meshlet_instances_count_buffer),
            std::move(visible_meshlet_instances_indices_buffer),
            std::move(meshlet_instance_visibility_mask_buffer),
            std::move(cull_triangles_cmd_buffer),
            std::move(debug_drawer_buffer)
        );

    //  ── CULL TRIANGLES ──────────────────────────────────────────────────
    auto vis_cull_triangles_pass = vuk::make_pass(
        stack.format("vis cull triangles {}", late ? "late" : "early"),
        [late, cull_flags](
            vuk::CommandBuffer &cmd_list,
            VUK_BA(vuk::eIndirectRead) cull_triangles_cmd,
            VUK_BA(vuk::eComputeRead) camera,
            VUK_BA(vuk::eComputeRead) visible_meshlet_instances_count,
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
                .bind_buffer(0, 1, visible_meshlet_instances_count)
                .bind_buffer(0, 2, visible_meshlet_instances_indices)
                .bind_buffer(0, 3, meshlet_instances)
                .bind_buffer(0, 4, mesh_instances)
                .bind_buffer(0, 5, meshes)
                .bind_buffer(0, 6, transforms)
                .bind_buffer(0, 7, draw_indexed_cmd)
                .bind_buffer(0, 8, reordered_indices)
                .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, cull_flags)
                .specialize_constants(0, late)
                .dispatch_indirect(cull_triangles_cmd);

            return std::make_tuple(
                camera,
                visible_meshlet_instances_count,
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

    std::tie(
        camera_buffer,
        visible_meshlet_instances_count_buffer,
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
            std::move(visible_meshlet_instances_count_buffer),
            std::move(visible_meshlet_instances_indices_buffer),
            std::move(meshlet_instances_buffer),
            std::move(mesh_instances_buffer),
            std::move(meshes_buffer),
            std::move(transforms_buffer),
            std::move(draw_command_buffer),
            std::move(reordered_indices_buffer)
        );

    return draw_command_buffer;
}

static auto draw_visbuffer(
    bool late,
    vuk::PersistentDescriptorSet &descriptor_set,
    vuk::Value<vuk::ImageAttachment> &depth_attachment,
    vuk::Value<vuk::ImageAttachment> &visbuffer_attachment,
    vuk::Value<vuk::ImageAttachment> &overdraw_attachment,
    vuk::Value<vuk::Buffer> &draw_command_buffer,
    vuk::Value<vuk::Buffer> &reordered_indices_buffer,
    vuk::Value<vuk::Buffer> &meshes_buffer,
    vuk::Value<vuk::Buffer> &mesh_instances_buffer,
    vuk::Value<vuk::Buffer> &meshlet_instances_buffer,
    vuk::Value<vuk::Buffer> &transforms_buffer,
    vuk::Value<vuk::Buffer> &materials_buffer,
    vuk::Value<vuk::Buffer> &camera_buffer
) -> void {
    ZoneScoped;
    memory::ScopedStack stack;

    auto vis_encode_pass = vuk::make_pass(
        stack.format("vis encode {}", late ? "late" : "early"),
        [&descriptor_set](
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
                .bind_persistent(1, descriptor_set)
                .bind_buffer(0, 0, camera)
                .bind_buffer(0, 1, meshlet_instances)
                .bind_buffer(0, 2, mesh_instances)
                .bind_buffer(0, 3, meshes)
                .bind_buffer(0, 4, transforms)
                .bind_buffer(0, 5, materials)
                .bind_image(0, 6, overdraw)
                .bind_index_buffer(index_buffer, vuk::IndexType::eUint32)
                .draw_indexed_indirect(1, triangle_indirect);

            return std::make_tuple(
                index_buffer,
                camera,
                meshlet_instances,
                mesh_instances,
                meshes,
                transforms,
                materials,
                visbuffer,
                depth,
                overdraw
            );
        }
    );

    std::tie(
        reordered_indices_buffer,
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
}

static auto draw_hiz(vuk::Value<vuk::ImageAttachment> &hiz_attachment, vuk::Value<vuk::ImageAttachment> &depth_attachment) -> void {
    ZoneScoped;

    auto hiz_generate_pass = vuk::make_pass(
        "hiz generate",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eComputeSampled) src,
           VUK_IA(vuk::eComputeRW) dst) {
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

    auto hiz_generate_slow_pass = vuk::make_pass(
        "hiz generate slow",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eComputeSampled) src,
           VUK_IA(vuk::eComputeRW) dst) {
            auto extent = dst->extent;
            auto mip_count = dst->level_count;

            cmd_list //
                .bind_compute_pipeline("passes.hiz_slow")
                .bind_sampler(0, 0, hiz_sampler_info);

            for (auto i = 0_u32; i < mip_count; i++) {
                auto mip_width = std::max(1_u32, extent.width >> i);
                auto mip_height = std::max(1_u32, extent.height >> i);

                auto mip = dst->mip(i);
                if (i == 0) {
                    cmd_list.bind_image(0, 1, src);
                } else {
                    auto mip = dst->mip(i - 1);
                    cmd_list.image_barrier(mip, vuk::eComputeWrite, vuk::eComputeSampled);
                    cmd_list.bind_image(0, 1, mip);
                }

                cmd_list.bind_image(0, 2, mip);
                cmd_list.push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(mip_width, mip_height, i));
                cmd_list.dispatch_invocations(mip_width, mip_height);
            }

            cmd_list.image_barrier(dst, vuk::eComputeSampled, vuk::eComputeRW);

            return std::make_tuple(src, dst);
        }
    );

    std::tie(depth_attachment, hiz_attachment) = hiz_generate_slow_pass(std::move(depth_attachment), std::move(hiz_attachment));
}

static auto draw_sky(
    SceneRenderer &self,
    vuk::Value<vuk::ImageAttachment> &dst_attachment,
    vuk::Value<vuk::ImageAttachment> &depth_attachment,
    vuk::Value<vuk::ImageAttachment> &sky_transmittance_lut_attachment,
    vuk::Value<vuk::ImageAttachment> &sky_multiscatter_lut_attachment,
    vuk::Value<vuk::Buffer> &environment_buffer,
    vuk::Value<vuk::Buffer> &camera_buffer
) -> void {
    ZoneScoped;

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
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
           VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
           VUK_BA(vuk::eComputeRead) environment,
           VUK_BA(vuk::eComputeRead) camera,
           VUK_IA(vuk::eComputeRW) sky_view_lut) {
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
                .bind_buffer(0, 3, environment)
                .bind_buffer(0, 4, camera)
                .bind_image(0, 5, sky_view_lut)
                .dispatch_invocations_per_pixel(sky_view_lut);
            return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, environment, camera, sky_view_lut);
        }
    );
    std::tie(sky_transmittance_lut_attachment, sky_multiscatter_lut_attachment, environment_buffer, camera_buffer, sky_view_lut_attachment) =
        sky_view_pass(
            std::move(sky_transmittance_lut_attachment),
            std::move(sky_multiscatter_lut_attachment),
            std::move(environment_buffer),
            std::move(camera_buffer),
            std::move(sky_view_lut_attachment)
        );

    //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
    auto sky_aerial_perspective_pass = vuk::make_pass(
        "sky aerial perspective",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eComputeSampled) sky_transmittance_lut,
           VUK_IA(vuk::eComputeSampled) sky_multiscatter_lut,
           VUK_BA(vuk::eComputeRead) environment,
           VUK_BA(vuk::eComputeRead) camera,
           VUK_IA(vuk::eComputeRW) sky_aerial_perspective_lut) {
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
                .bind_buffer(0, 3, environment)
                .bind_buffer(0, 4, camera)
                .bind_image(0, 5, sky_aerial_perspective_lut)
                .dispatch_invocations_per_pixel(sky_aerial_perspective_lut);
            return std::make_tuple(sky_transmittance_lut, sky_multiscatter_lut, environment, camera, sky_aerial_perspective_lut);
        }
    );

    std::tie(
        sky_transmittance_lut_attachment,
        sky_multiscatter_lut_attachment,
        environment_buffer,
        camera_buffer,
        sky_aerial_perspective_attachment
    ) =
        sky_aerial_perspective_pass(
            std::move(sky_transmittance_lut_attachment),
            std::move(sky_multiscatter_lut_attachment),
            std::move(environment_buffer),
            std::move(camera_buffer),
            std::move(sky_aerial_perspective_attachment)
        );

    //  ── SKY FINAL ───────────────────────────────────────────────────────
    auto sky_final_pass = vuk::make_pass(
        "sky final",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eColorRW) dst,
           VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
           VUK_IA(vuk::eFragmentSampled) sky_aerial_perspective_lut,
           VUK_IA(vuk::eFragmentSampled) sky_view_lut,
           VUK_IA(vuk::eFragmentSampled) depth,
           VUK_BA(vuk::eFragmentRead) environment,
           VUK_BA(vuk::eFragmentRead) camera) {
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
                .bind_buffer(0, 5, environment)
                .bind_buffer(0, 6, camera)
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth, environment, camera);
        }
    );

    std::tie(dst_attachment, depth_attachment, environment_buffer, camera_buffer) = sky_final_pass(
        std::move(dst_attachment),
        std::move(sky_transmittance_lut_attachment),
        std::move(sky_aerial_perspective_attachment),
        std::move(sky_view_lut_attachment),
        std::move(depth_attachment),
        std::move(environment_buffer),
        std::move(camera_buffer)
    );
}

auto SceneRenderer::render(this SceneRenderer &self, vuk::Value<vuk::ImageAttachment> &&dst_attachment, SceneRenderInfo &info, PreparedFrame &frame)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto &device = App::mod<Device>();
    auto &transfer_man = device.transfer_man();
    auto &bindless_descriptor_set = device.get_descriptor_set();

    //   ──────────────────────────────────────────────────────────────────────
    auto final_attachment = vuk::declare_ia(
        "final",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
          .format = vuk::Format::eB10G11R11UfloatPack32,
          .sample_count = vuk::Samples::e1 }
    );
    final_attachment.same_shape_as(dst_attachment);
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia(
        "depth",
        { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eDepthStencilAttachment,
          .format = vuk::Format::eD32Sfloat,
          .sample_count = vuk::Samples::e1 }
    );
    depth_attachment.same_shape_as(final_attachment);
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::DepthZero);

    auto hiz_extent = vuk::Extent3D{
        .width = (dst_attachment->extent.width + 63_u32) & ~63_u32,
        .height = (dst_attachment->extent.height + 63_u32) & ~63_u32,
        .depth = 1,
    };

    auto hiz_attachment = vuk::Value<vuk::ImageAttachment>{};
    if (self.hiz.extent() != hiz_extent || !self.hiz) {
        if (self.hiz_view) {
            device.destroy(self.hiz_view.id());
        }

        if (self.hiz) {
            device.destroy(self.hiz.id());
        }

        auto hiz_info = ImageInfo{
            .format = vuk::Format::eR32Sfloat,
            .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
            .type = vuk::ImageType::e2D,
            .extent = hiz_extent,
            .mip_count = std::bit_width(ls::max(hiz_extent.width, hiz_extent.height)) - 1_u32,
            .name = "HiZ",
        };
        self.hiz = Image::create(device, hiz_info).value();

        auto hiz_view_info = ImageViewInfo{
            .image_usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
            .type = vuk::ImageViewType::e2D,
            .subresource_range = { .aspectMask = vuk::ImageAspectFlagBits::eColor, .levelCount = hiz_info.mip_count },
            .name = "HiZ View",
        };
        self.hiz_view = ImageView::create(device, self.hiz, hiz_view_info).value();

        hiz_attachment = self.hiz_view.acquire(device, "HiZ", vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage, vuk::eNone);
        hiz_attachment = vuk::clear_image(std::move(hiz_attachment), vuk::DepthZero);
    } else {
        hiz_attachment =
            self.hiz_view.acquire(device, "HiZ", vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage, vuk::eComputeSampled);
    }

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

    auto environment_buffer = std::move(frame.environment_buffer);
    auto camera_buffer = std::move(frame.camera_buffer);
    auto sky_transmittance_lut_attachment = std::move(frame.sky_transmittance_lut);
    auto sky_multiscatter_lut_attachment = std::move(frame.sky_multiscatter_lut);

    if (frame.mesh_instance_count) {
        auto visbuffer_attachment = vuk::declare_ia(
            "visbuffer",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
              .format = vuk::Format::eR32Uint,
              .sample_count = vuk::Samples::e1 }
        );
        visbuffer_attachment.same_shape_as(final_attachment);

        auto overdraw_attachment = vuk::declare_ia(
            "overdraw",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
              .format = vuk::Format::eR32Uint,
              .sample_count = vuk::Samples::e1 }
        );
        overdraw_attachment.same_shape_as(final_attachment);

        auto vis_clear_pass = vuk::make_pass(
            "vis clear",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eComputeWrite) visbuffer,
               VUK_IA(vuk::eComputeWrite) overdraw) {
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

        std::tie(visbuffer_attachment, overdraw_attachment) = vis_clear_pass(std::move(visbuffer_attachment), std::move(overdraw_attachment));

        auto transforms_buffer = std::move(frame.transforms_buffer);
        auto meshes_buffer = std::move(frame.meshes_buffer);
        auto mesh_instances_buffer = std::move(frame.mesh_instances_buffer);
        auto materials_buffer = std::move(frame.materials_buffer);
        auto meshlet_instance_visibility_mask_buffer = std::move(frame.meshlet_instance_visibility_mask_buffer);

        auto meshlet_instances_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, frame.max_meshlet_instance_count * sizeof(GPU::MeshletInstance));
        auto visible_meshlet_instances_indices_buffer =
            transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, frame.max_meshlet_instance_count * sizeof(u32));
        auto reordered_indices_buffer = transfer_man.alloc_transient_buffer(
            vuk::MemoryUsage::eGPUonly,
            frame.max_meshlet_instance_count * Model::MAX_MESHLET_PRIMITIVES * 3 * sizeof(u32)
        );

        auto visible_meshlet_instances_count_buffer = transfer_man.scratch_buffer<u32[3]>({});
        auto cull_meshlets_cmd_buffer = cull_meshes(
            info.cull_flags,
            frame.mesh_instance_count,
            transfer_man,
            meshes_buffer,
            mesh_instances_buffer,
            meshlet_instances_buffer,
            visible_meshlet_instances_count_buffer,
            transforms_buffer,
            hiz_attachment,
            camera_buffer,
            debug_drawer_buffer
        );

        auto early_draw_visbuffer_cmd_buffer = cull_meshlets(
            false,
            info.cull_flags,
            transfer_man,
            hiz_attachment,
            cull_meshlets_cmd_buffer,
            visible_meshlet_instances_count_buffer,
            visible_meshlet_instances_indices_buffer,
            meshlet_instance_visibility_mask_buffer,
            reordered_indices_buffer,
            meshes_buffer,
            mesh_instances_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            camera_buffer,
            debug_drawer_buffer
        );

        draw_visbuffer(
            false,
            bindless_descriptor_set,
            depth_attachment,
            visbuffer_attachment,
            overdraw_attachment,
            early_draw_visbuffer_cmd_buffer,
            reordered_indices_buffer,
            meshes_buffer,
            mesh_instances_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            materials_buffer,
            camera_buffer
        );

        draw_hiz(hiz_attachment, depth_attachment);

        auto late_draw_visbuffer_cmd_buffer = cull_meshlets(
            true,
            info.cull_flags,
            transfer_man,
            hiz_attachment,
            cull_meshlets_cmd_buffer,
            visible_meshlet_instances_count_buffer,
            visible_meshlet_instances_indices_buffer,
            meshlet_instance_visibility_mask_buffer,
            reordered_indices_buffer,
            meshes_buffer,
            mesh_instances_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            camera_buffer,
            debug_drawer_buffer
        );

        draw_visbuffer(
            true,
            bindless_descriptor_set,
            depth_attachment,
            visbuffer_attachment,
            overdraw_attachment,
            late_draw_visbuffer_cmd_buffer,
            reordered_indices_buffer,
            meshes_buffer,
            mesh_instances_buffer,
            meshlet_instances_buffer,
            transforms_buffer,
            materials_buffer,
            camera_buffer
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
            picked_texel.wait(device.get_allocator(), temp_compiler);

            u32 texel_data = 0;
            std::memcpy(&texel_data, picked_texel->mapped_ptr, sizeof(u32));
            info.picked_transform_index = texel_data;
        }

        if (self.overdraw_heatmap_scale > 0.0f) {
            auto visualize_overdraw_pass = vuk::make_pass(
                "visualize overdraw",
                [scale = self.overdraw_heatmap_scale](
                    vuk::CommandBuffer &cmd_list, //
                    VUK_IA(vuk::eColorRW) dst,
                    VUK_IA(vuk::eFragmentSampled) overdraw
                ) {
                    cmd_list //
                        .bind_graphics_pipeline("passes.visualize_overdraw")
                        .set_rasterization({})
                        .set_color_blend(dst, vuk::BlendPreset::eOff)
                        .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                        .set_viewport(0, vuk::Rect2D::framebuffer())
                        .set_scissor(0, vuk::Rect2D::framebuffer())
                        .bind_image(0, 0, overdraw)
                        .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, scale)
                        .draw(3, 1, 0, 0);

                    return dst;
                }
            );

            return visualize_overdraw_pass(std::move(dst_attachment), std::move(overdraw_attachment));
        }

        //  ── VISBUFFER DECODE ────────────────────────────────────────────────
        auto vis_decode_pass = vuk::make_pass(
            "vis decode",
            [descriptor_set = &bindless_descriptor_set]( //
                vuk::CommandBuffer &cmd_list,
                VUK_BA(vuk::eFragmentRead) camera,
                VUK_BA(vuk::eFragmentRead) meshlet_instances,
                VUK_BA(vuk::eFragmentRead) mesh_instances,
                VUK_BA(vuk::eFragmentRead) meshes,
                VUK_BA(vuk::eFragmentRead) transforms,
                VUK_BA(vuk::eFragmentRead) materials,
                VUK_IA(vuk::eFragmentSampled) visbuffer,
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

        // VBGTAO
        auto vbgtao_occlusion_attachment = vuk::declare_ia(
            "vbgtao occlusion",
            { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
              .format = vuk::Format::eR16Sfloat,
              .sample_count = vuk::Samples::e1 }
        );
        vbgtao_occlusion_attachment.same_shape_as(final_attachment);
        vbgtao_occlusion_attachment = vuk::clear_image(std::move(vbgtao_occlusion_attachment), vuk::White<f32>);

        if (true) {
            auto vbgtao_prefilter_pass = vuk::make_pass(
                "vbgtao prefilter",
                [](vuk::CommandBuffer &command_buffer, //
                   VUK_IA(vuk::eComputeSampled) depth_input,
                   VUK_IA(vuk::eComputeRW) dst_image) {
                    auto nearest_clamp_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eNearest,
                        .minFilter = vuk::Filter::eNearest,
                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                    };

                    command_buffer.bind_compute_pipeline("passes.vbgtao_prefilter")
                        .bind_image(0, 0, depth_input)
                        .bind_image(0, 1, dst_image->mip(0))
                        .bind_image(0, 2, dst_image->mip(1))
                        .bind_image(0, 3, dst_image->mip(2))
                        .bind_image(0, 4, dst_image->mip(3))
                        .bind_image(0, 5, dst_image->mip(4))
                        .bind_sampler(0, 6, nearest_clamp_sampler)
                        .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, depth_input->extent)
                        .dispatch((depth_input->extent.width + 16 - 1) / 16, (depth_input->extent.height + 16 - 1) / 16);

                    return std::make_tuple(depth_input, dst_image);
                }
            );

            auto vbgtao_depth_attachment = vuk::declare_ia(
                "vbgtao depth",
                { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
                  .format = vuk::Format::eR32Sfloat,
                  .sample_count = vuk::Samples::e1,
                  .level_count = 5,
                  .layer_count = 1 }
            );
            vbgtao_depth_attachment.same_extent_as(depth_attachment);
            vbgtao_depth_attachment = vuk::clear_image(std::move(vbgtao_depth_attachment), vuk::Black<f32>);

            std::tie(depth_attachment, vbgtao_depth_attachment) =
                vbgtao_prefilter_pass(std::move(depth_attachment), std::move(vbgtao_depth_attachment));

            auto vbgtao_generate_pass = vuk::make_pass(
                "vbgtao generate",
                [](vuk::CommandBuffer &command_buffer, //
                   VUK_BA(vuk::eComputeUniformRead) camera,
                   VUK_IA(vuk::eComputeSampled) prefiltered_depth,
                   VUK_IA(vuk::eComputeSampled) normals,
                   VUK_IA(vuk::eComputeSampled) hilbert_noise,
                   VUK_IA(vuk::eComputeRW) ambient_occlusion,
                   VUK_IA(vuk::eComputeRW) depth_differences) {
                    auto nearest_clamp_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eNearest,
                        .minFilter = vuk::Filter::eNearest,
                        .mipmapMode = vuk::SamplerMipmapMode::eNearest,
                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                    };

                    auto linear_clamp_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eLinear,
                        .minFilter = vuk::Filter::eLinear,
                        .mipmapMode = vuk::SamplerMipmapMode::eLinear,
                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                    };

                    command_buffer.bind_compute_pipeline("passes.vbgtao_generate")
                        .bind_buffer(0, 0, camera)
                        .bind_image(0, 1, prefiltered_depth)
                        .bind_image(0, 2, normals)
                        .bind_image(0, 3, hilbert_noise)
                        .bind_image(0, 4, ambient_occlusion)
                        .bind_image(0, 5, depth_differences)
                        .bind_sampler(0, 6, nearest_clamp_sampler)
                        .bind_sampler(0, 7, linear_clamp_sampler)
                        .dispatch_invocations_per_pixel(ambient_occlusion);

                    return std::make_tuple(camera, normals, ambient_occlusion, depth_differences);
                }
            );

            auto vbgtao_noisy_occlusion_attachment = vuk::declare_ia(
                "vbgtao noisy occlusion",
                { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage, .sample_count = vuk::Samples::e1 }
            );
            vbgtao_noisy_occlusion_attachment.same_format_as(vbgtao_occlusion_attachment);
            vbgtao_noisy_occlusion_attachment.same_shape_as(vbgtao_occlusion_attachment);
            vbgtao_noisy_occlusion_attachment = vuk::clear_image(std::move(vbgtao_noisy_occlusion_attachment), vuk::White<f32>);

            auto vbgtao_depth_differences_attachment = vuk::declare_ia(
                "vbgtao depth differences",
                { .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
                  .format = vuk::Format::eR32Uint,
                  .sample_count = vuk::Samples::e1 }
            );
            vbgtao_depth_differences_attachment.same_shape_as(final_attachment);
            vbgtao_depth_differences_attachment = vuk::clear_image(std::move(vbgtao_depth_differences_attachment), vuk::Black<f32>);

            auto hilbert_noise_lut_attachment =
                self.hilbert_noise_lut_view.acquire(device, "hilbert noise", vuk::ImageUsageFlagBits::eSampled, vuk::eComputeSampled);

            std::tie(camera_buffer, normal_attachment, vbgtao_noisy_occlusion_attachment, vbgtao_depth_differences_attachment) = vbgtao_generate_pass(
                std::move(camera_buffer),
                std::move(vbgtao_depth_attachment),
                std::move(normal_attachment),
                std::move(hilbert_noise_lut_attachment),
                std::move(vbgtao_noisy_occlusion_attachment),
                std::move(vbgtao_depth_differences_attachment)
            );

            auto vbgtao_denoise_pass = vuk::make_pass(
                "vbgtao denoise",
                [](vuk::CommandBuffer &command_buffer, //
                   VUK_IA(vuk::eComputeSampled) noisy_occlusion,
                   VUK_IA(vuk::eComputeSampled) depth_differences,
                   VUK_IA(vuk::eComputeRW) ambient_occlusion) {
                    auto nearest_clamp_sampler = vuk::SamplerCreateInfo{
                        .magFilter = vuk::Filter::eNearest,
                        .minFilter = vuk::Filter::eNearest,
                        .mipmapMode = vuk::SamplerMipmapMode::eNearest,
                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                        .addressModeW = vuk::SamplerAddressMode::eClampToEdge,
                    };

                    command_buffer.bind_compute_pipeline("passes.vbgtao_denoise")
                        .bind_image(0, 0, noisy_occlusion)
                        .bind_image(0, 1, depth_differences)
                        .bind_image(0, 2, ambient_occlusion)
                        .bind_sampler(0, 3, nearest_clamp_sampler)
                        .dispatch_invocations_per_pixel(ambient_occlusion);

                    return ambient_occlusion;
                }
            );

            vbgtao_occlusion_attachment = vbgtao_denoise_pass(
                std::move(vbgtao_noisy_occlusion_attachment),
                std::move(vbgtao_depth_differences_attachment),
                std::move(vbgtao_occlusion_attachment)
            );
        }

        //  ── BRDF ────────────────────────────────────────────────────────────
        auto brdf_pass = vuk::make_pass(
            "brdf",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eColorRW) dst,
               VUK_BA(vuk::eFragmentRead) environment,
               VUK_BA(vuk::eFragmentRead) camera,
               VUK_IA(vuk::eFragmentSampled) sky_transmittance_lut,
               VUK_IA(vuk::eFragmentSampled) sky_multiscatter_lut,
               VUK_IA(vuk::eFragmentSampled) depth,
               VUK_IA(vuk::eFragmentSampled) ambient_occlusion,
               VUK_IA(vuk::eFragmentSampled) albedo,
               VUK_IA(vuk::eFragmentSampled) normal,
               VUK_IA(vuk::eFragmentSampled) emissive,
               VUK_IA(vuk::eFragmentSampled) metallic_roughness_occlusion) {
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
                    .bind_image(0, 5, ambient_occlusion)
                    .bind_image(0, 6, albedo)
                    .bind_image(0, 7, normal)
                    .bind_image(0, 8, emissive)
                    .bind_image(0, 9, metallic_roughness_occlusion)
                    .bind_buffer(0, 10, environment)
                    .bind_buffer(0, 11, camera)
                    .draw(3, 1, 0, 0);

                return std::make_tuple(dst, environment, camera, sky_transmittance_lut, sky_multiscatter_lut, depth);
            }
        );

        std::tie(
            final_attachment,
            environment_buffer,
            camera_buffer,
            sky_transmittance_lut_attachment,
            sky_multiscatter_lut_attachment,
            depth_attachment
        ) =
            brdf_pass(
                std::move(final_attachment),
                std::move(environment_buffer),
                std::move(camera_buffer),
                std::move(sky_transmittance_lut_attachment),
                std::move(sky_multiscatter_lut_attachment),
                std::move(depth_attachment),
                std::move(vbgtao_occlusion_attachment),
                std::move(albedo_attachment),
                std::move(normal_attachment),
                std::move(emissive_attachment),
                std::move(metallic_roughness_occlusion_attachment)
            );
    }

    if (frame.environment_flags & (GPU::EnvironmentFlags::HasSun | GPU::EnvironmentFlags::HasAtmosphere)) {
        draw_sky(
            self,
            final_attachment,
            depth_attachment,
            sky_transmittance_lut_attachment,
            sky_multiscatter_lut_attachment,
            environment_buffer,
            camera_buffer
        );
    }

    auto histogram_luminance_buffer = self.histogram_luminance_buffer.acquire(device, "histogram luminance", vuk::eFragmentRead);
    if (frame.environment_flags & GPU::EnvironmentFlags::HasEyeAdaptation) {
        //  ── HISTOGRAM GENERATE ──────────────────────────────────────────────
        auto histogram_generate_pass = vuk::make_pass(
            "histogram generate",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eComputeSampled) src_image,
               VUK_BA(vuk::eComputeRead) environment,
               VUK_BA(vuk::eComputeRW) histogram_bin_indices) {
                cmd_list.bind_compute_pipeline("passes.histogram_generate")
                    .bind_image(0, 0, src_image)
                    .bind_buffer(0, 1, environment)
                    .bind_buffer(0, 2, histogram_bin_indices)
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, src_image->extent)
                    .dispatch_invocations_per_pixel(src_image);

                return std::make_tuple(src_image, environment, histogram_bin_indices);
            }
        );

        auto histogram_bin_indices_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, GPU::HISTOGRAM_BIN_COUNT * sizeof(u32));
        vuk::fill(histogram_bin_indices_buffer, 0);
        std::tie(final_attachment, environment_buffer, histogram_bin_indices_buffer) =
            histogram_generate_pass(std::move(final_attachment), std::move(environment_buffer), std::move(histogram_bin_indices_buffer));

        //  ── HISTOGRAM AVERAGE ───────────────────────────────────────────────
        auto histogram_average_pass = vuk::make_pass(
            "histogram average",
            [pixel_count = f32(dst_attachment->extent.width * dst_attachment->extent.height), delta_time = info.delta_time](
                vuk::CommandBuffer &cmd_list, //
                VUK_BA(vuk::eComputeRead) environment,
                VUK_BA(vuk::eComputeRead) histogram_bin_indices,
                VUK_BA(vuk::eComputeRW) histogram_luminance
            ) {
                cmd_list //
                    .bind_compute_pipeline("passes.histogram_average")
                    .bind_buffer(0, 0, environment)
                    .bind_buffer(0, 1, histogram_bin_indices)
                    .bind_buffer(0, 2, histogram_luminance)
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, PushConstants(pixel_count, delta_time))
                    .dispatch(1);

                return std::make_tuple(environment, histogram_luminance);
            }
        );

        std::tie(environment_buffer, histogram_luminance_buffer) =
            histogram_average_pass(std::move(environment_buffer), std::move(histogram_bin_indices_buffer), std::move(histogram_luminance_buffer));
    }

    //  ── TONEMAP ─────────────────────────────────────────────────────────
    auto tonemap_pass = vuk::make_pass(
        "tonemap",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eColorRW) dst,
           VUK_IA(vuk::eFragmentSampled) src,
           VUK_BA(vuk::eFragmentRead) environment,
           VUK_BA(vuk::eFragmentUniformRead) histogram_luminance) {
            cmd_list //
                .bind_graphics_pipeline("passes.tonemap")
                .set_rasterization({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, src)
                .bind_buffer(0, 2, environment)
                .bind_buffer(0, 3, histogram_luminance)
                .draw(3, 1, 0, 0);

            return dst;
        }
    );
    dst_attachment =
        tonemap_pass(std::move(dst_attachment), std::move(final_attachment), std::move(environment_buffer), std::move(histogram_luminance_buffer));

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [](vuk::CommandBuffer &cmd_list, //
           VUK_IA(vuk::eColorRW) dst,
           VUK_IA(vuk::eDepthStencilRW) depth,
           VUK_BA(vuk::eVertexRead | vuk::eFragmentRead) camera) {
            cmd_list //
                .bind_graphics_pipeline("passes.editor_grid")
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .bind_buffer(0, 0, camera)
                .draw(3, 1, 0, 0);

            return std::make_tuple(dst, depth);
        }
    );
    // std::tie(dst_attachment, depth_attachment) = editor_grid_pass(std::move(dst_attachment), std::move(depth_attachment), std::move(camera_buffer));

    if (debugging) {
        auto debug_pass = vuk::make_pass(
            "debug pass",
            [](vuk::CommandBuffer &cmd_list, //
               VUK_IA(vuk::eColorRW) dst,
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

        dst_attachment = debug_pass(
            std::move(dst_attachment),
            std::move(debug_drawer_buffer),
            std::move(camera_buffer),
            std::move(debug_draw_aabb_buffer),
            std::move(debug_draw_rect_buffer)
        );
    }

    return dst_attachment;
}

auto SceneRenderer::cleanup(this SceneRenderer &self) -> void {
    ZoneScoped;

    auto &device = App::mod<Device>();

    device.wait();

    if (self.transforms_buffer) {
        device.destroy(self.transforms_buffer.id());
        self.transforms_buffer = {};
    }

    if (self.mesh_instances_buffer) {
        device.destroy(self.mesh_instances_buffer.id());
        self.mesh_instances_buffer = {};
    }

    if (self.meshes_buffer) {
        device.destroy(self.meshes_buffer.id());
        self.meshes_buffer = {};
    }

    if (self.meshlet_instance_visibility_mask_buffer) {
        device.destroy(self.meshlet_instance_visibility_mask_buffer.id());
        self.meshlet_instance_visibility_mask_buffer = {};
    }

    if (self.materials_buffer) {
        device.destroy(self.materials_buffer.id());
        self.materials_buffer = {};
    }

    if (self.hiz_view) {
        device.destroy(self.hiz_view.id());
        self.hiz_view = {};
    }

    if (self.hiz) {
        device.destroy(self.hiz.id());
        self.hiz = {};
    }
}

} // namespace lr
