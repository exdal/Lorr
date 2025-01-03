#include "Engine/World/WorldRenderer.hh"

#include "Engine/Core/Application.hh"

#include <imgui.h>

namespace lr {
template<>
struct Handle<WorldRenderer>::Impl {
    Device *device = nullptr;
    WorldRenderContext context = {};

    Sampler linear_sampler = {};
    Sampler nearest_sampler = {};

    Pipeline grid_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Pipeline sky_view_pipeline = {};
    Pipeline sky_aerial_perspective_pipeline = {};
    Pipeline sky_final_pipeline = {};

    ls::option<vuk::Value<vuk::ImageAttachment>> composition_result = {};
};

auto WorldRenderer::create(Device *device) -> WorldRenderer {
    auto impl = new Impl;
    auto self = WorldRenderer(impl);
    impl->device = device;

    self.setup_persistent_resources();

    // TODO: Remove later
    impl->context.sun_angle = { 90, 45 };
    self.update_sun_dir();

    return self;
}

auto WorldRenderer::setup_persistent_resources() -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &transfer_man = app.device.transfer_man();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    // clang-format off
    impl->linear_sampler = Sampler::create(
        *impl->device,
        vuk::Filter::eLinear,
        vuk::Filter::eLinear,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::CompareOp::eNever)
        .value();
    impl->nearest_sampler = Sampler::create(
        *impl->device,
        vuk::Filter::eNearest,
        vuk::Filter::eNearest,
        vuk::SamplerMipmapMode::eNearest,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::CompareOp::eNever)
        .value();

    //  ── GRID ────────────────────────────────────────────────────────────
    impl->grid_pipeline = Pipeline::create(*impl->device, {
        .module_name = "editor.grid",
        .root_path = shaders_root,
        .shader_path = shaders_root / "editor" / "grid.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    //  ── SKY ─────────────────────────────────────────────────────────────
    impl->sky_transmittance_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(256, 64, 1))
        .value();
    impl->sky_transmittance_lut_view = ImageView::create(
        *impl->device,
        impl->sky_transmittance_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->sky_multiscatter_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(32, 32, 1))
        .value();
    impl->sky_multiscatter_lut_view = ImageView::create(
        *impl->device,
        impl->sky_multiscatter_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->sky_view_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.lut",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "lut.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    impl->sky_aerial_perspective_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.aerial_perspective",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "aerial_perspective.slang",
        .entry_points = { "cs_main" },
    }).value();
    impl->sky_final_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.final",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "final.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    auto sky_transmittance_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.transmittance",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "transmittance.slang",
        .entry_points = { "cs_main" },
    }).value();
    auto sky_multiscatter_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.ms",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "ms.slang",
        .entry_points = { "cs_main" },
    }).value();
    // clang-format on

    //  ── DEFINE PASSES ───────────────────────────────────────────────────
    auto transmittance_lut_pass = vuk::make_pass(
        "transmittance lut",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(sky_transmittance_pipeline.id())](
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
                        .world_ptr = ctx.world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return dst;
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(sky_multiscatter_pipeline.id())](
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
                        .world_ptr = ctx.world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst);
        },
        vuk::DomainFlagBits::eGraphicsQueue);
    //   ──────────────────────────────────────────────────────────────────────

    auto transmittance_lut_attachment =
        impl->sky_transmittance_lut_view.discard(*impl->device, "sky_transmittance_lut", vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_lut_attachment =
        impl->sky_multiscatter_lut_view.discard(*impl->device, "sky_multiscatter_lut", vuk::ImageUsageFlagBits::eStorage);

    auto temp_scene_info = this->begin_scene({});
    this->end_scene(temp_scene_info);

    transmittance_lut_attachment = transmittance_lut_pass(std::move(transmittance_lut_attachment));
    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment) =
        multiscatter_lut_pass(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));

    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));
}

auto WorldRenderer::begin_scene(const SceneBeginInfo &info) -> GPUSceneData {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();
    GPUSceneData result;

    usize world_data_bytes = sizeof(GPUWorldData);
    usize cameras_size_bytes = sizeof(GPUCameraData) * info.camera_count;
    usize model_transforms_bytes = sizeof(GPUModelTransformData) * info.model_transform_count;
    usize buffer_size = world_data_bytes + cameras_size_bytes + model_transforms_bytes;

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

    LS_EXPECT(result.upload_size == buffer_size);

    return result;
}

auto WorldRenderer::end_scene(GPUSceneData &scene_gpu_data) -> void {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();
    auto gpu_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, scene_gpu_data.upload_size);
    auto materials_buffer_ptr = scene_gpu_data.materials_buffer_id != BufferID::Invalid
                                    ? impl->device->buffer(scene_gpu_data.materials_buffer_id)->device_address
                                    : 0;

    auto &world_data = impl->context.world_data;
    world_data.linear_sampler_id = impl->linear_sampler.id();
    world_data.nearest_sampler_id = impl->nearest_sampler.id();

    if (scene_gpu_data.offset_cameras.has_value()) {
        world_data.cameras_ptr = gpu_buffer.device_address() + scene_gpu_data.offset_cameras.value();
    }
    world_data.materials_ptr = materials_buffer_ptr;
    if (scene_gpu_data.offset_model_transforms.has_value()) {
        world_data.model_transforms_ptr = gpu_buffer.device_address() + scene_gpu_data.offset_model_transforms.value();
    }
    world_data.active_camera_index = scene_gpu_data.active_camera_index;

    impl->context.world_ptr = gpu_buffer.device_address();
    std::memcpy(scene_gpu_data.world_data.host_ptr<GPUWorldData>(), &impl->context.world_data, sizeof(GPUWorldData));

    transfer_man.upload_staging(scene_gpu_data.world_data, gpu_buffer);
}

auto WorldRenderer::render(vuk::Value<vuk::ImageAttachment> &render_target) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto final_attachment = vuk::declare_ia("final", { .format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::Samples::e1 });
    final_attachment.same_shape_as(render_target);
    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);

    auto depth_attachment = vuk::declare_ia("depth", { .format = vuk::Format::eD32Sfloat, .sample_count = vuk::Samples::e1 });
    depth_attachment.same_shape_as(render_target);
    depth_attachment = vuk::clear_image(std::move(depth_attachment), vuk::ClearDepth(1.0));

    //  ── SKY VIEW LUT ────────────────────────────────────────────────────
    auto sky_view_pass = vuk::make_pass(
        "sky view",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->sky_view_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, multiscatter_lut)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, ctx.world_ptr)
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut);
        });

    auto transmittance_lut_attachment = impl->sky_transmittance_lut_view.acquire(
        *impl->device, "transmittance_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
    auto multiscatter_lut_attachment = impl->sky_multiscatter_lut_view.acquire(
        *impl->device, "multiscatter_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

    auto sky_view_lut_attachment =
        vuk::declare_ia("sky_view_lut", { .extent = { 200, 100, 1 }, .sample_count = vuk::Samples::e1, .layer_count = 1 });
    sky_view_lut_attachment.same_format_as(final_attachment);
    sky_view_lut_attachment = vuk::clear_image(std::move(sky_view_lut_attachment), vuk::Black<f32>);
    std::tie(sky_view_lut_attachment, transmittance_lut_attachment, multiscatter_lut_attachment) =
        sky_view_pass(std::move(sky_view_lut_attachment), std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));

    //  ── SKY AERIAL PERSPECTIVE ──────────────────────────────────────────
    auto sky_aerial_perspective_pass = vuk::make_pass(
        "sky aerial perspective",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->sky_aerial_perspective_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eComputeWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut) {
            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec3 image_size = {};
                u32 pad = 0;
            };

            auto image_size = glm::vec3(dst->extent.width, dst->extent.height, dst->extent.height);
            auto groups = glm::uvec2(image_size) / glm::uvec2(16);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, dst)
                .bind_image(0, 2, transmittance_lut)
                .bind_image(0, 3, multiscatter_lut)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = ctx.world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, static_cast<u32>(image_size.z));
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut);
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto sky_aerial_perspective_attachment = vuk::declare_ia(
        "sky_aerial_perspective",
        { .image_type = vuk::ImageType::e3D,
          .extent = { 32, 32, 32 },
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
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->sky_final_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) sky_view_lut,
            VUK_IA(vuk::Access::eFragmentSampled) aerial_perspective_lut,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, sky_view_lut)
                .set_rasterization({})
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, ctx.world_ptr)
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, sky_view_lut, aerial_perspective_lut, transmittance_lut);
        });

    std::tie(final_attachment, sky_view_lut_attachment, sky_aerial_perspective_attachment, transmittance_lut_attachment) = sky_final_pass(
        std::move(final_attachment),
        std::move(sky_view_lut_attachment),
        std::move(sky_aerial_perspective_attachment),
        std::move(transmittance_lut_attachment));

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->grid_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst, VUK_IA(vuk::Access::eDepthStencilRW) dst_depth) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, ctx.world_ptr)
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, dst_depth);
        });

    std::tie(final_attachment, depth_attachment) = editor_grid_pass(std::move(final_attachment), std::move(depth_attachment));

    return final_attachment;
}

auto WorldRenderer::draw_profiler_ui() -> void {
    ZoneScoped;
}

auto WorldRenderer::sun_angle() -> glm::vec2 & {
    ZoneScoped;

    return impl->context.sun_angle;
}

auto WorldRenderer::update_sun_dir() -> void {
    ZoneScoped;

    auto rad = glm::radians(impl->context.sun_angle);
    this->world_data().sun.direction = {
        glm::cos(rad.x) * glm::sin(rad.y),
        glm::sin(rad.x) * glm::sin(rad.y),
        glm::cos(rad.y),
    };
}

auto WorldRenderer::world_data() -> GPUWorldData & {
    return impl->context.world_data;
}

}  // namespace lr
