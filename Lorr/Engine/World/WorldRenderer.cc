#include "Engine/World/WorldRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/World/Tasks/Grid.hh"
#include "Engine/World/Tasks/ImGui.hh"
#include "Engine/World/Tasks/Sky.hh"

#include <glm/gtx/component_wise.hpp>

namespace lr {
template<>
struct Handle<WorldRenderer>::Impl {
    Device *device = nullptr;
    WorldRenderContext context = {};
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
    impl->context.linear_sampler = Sampler::create(
        *impl->device,
        vuk::Filter::eLinear,
        vuk::Filter::eLinear,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::CompareOp::eNever)
        .value();
    impl->context.nearest_sampler = Sampler::create(
        *impl->device,
        vuk::Filter::eNearest,
        vuk::Filter::eNearest,
        vuk::SamplerMipmapMode::eNearest,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::CompareOp::eNever)
        .value();

    //  ── IMGUI ───────────────────────────────────────────────────────────
    auto [imgui_atlas_data, imgui_atlas_dimensions] = Tasks::imgui_build_font_atlas(asset_man);
    impl->context.imgui_font_image = Image::create(
        *impl->device,
        vuk::Format::eR8G8B8A8Unorm,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageType::e2D,
        vuk::Extent3D(imgui_atlas_dimensions.x, imgui_atlas_dimensions.y, 1u))
        .value();
    impl->context.imgui_font_view = ImageView::create(
        *impl->device,
        impl->context.imgui_font_image,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    transfer_man.upload_staging(impl->context.imgui_font_view, imgui_atlas_data, vuk::Access::eFragmentSampled);
    impl->context.imgui_pipeline = Pipeline::create(*impl->device, {
        .module_name = "imgui",
        .root_path = shaders_root,
        .shader_path = shaders_root / "imgui.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    //  ── GRID ────────────────────────────────────────────────────────────
    impl->context.grid_pipeline = Pipeline::create(*impl->device, {
        .module_name = "editor.grid",
        .root_path = shaders_root,
        .shader_path = shaders_root / "editor" / "grid.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    //  ── SKY ─────────────────────────────────────────────────────────────
    impl->context.sky_transmittance_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(256, 64, 1))
        .value();
    impl->context.sky_transmittance_lut_view = ImageView::create(
        *impl->device,
        impl->context.sky_transmittance_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->context.sky_multiscatter_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(32, 32, 1))
        .value();
    impl->context.sky_multiscatter_lut_view = ImageView::create(
        *impl->device,
        impl->context.sky_multiscatter_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->context.sky_aerial_perspective_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e3D,
        vuk::Extent3D(32, 32, 32))
        .value();
    impl->context.sky_aerial_perspective_lut_view = ImageView::create(
        *impl->device,
        impl->context.sky_aerial_perspective_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e3D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->context.sky_view_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        vuk::ImageType::e2D,
        vuk::Extent3D(200, 100, 1))
        .value();
    impl->context.sky_view_lut_view = ImageView::create(
        *impl->device,
        impl->context.sky_view_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->context.sky_view_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.lut",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "lut.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    // clang-format on

    // Prepare LUTs
    auto temp_scene_info = this->begin_scene({});
    this->end_scene(temp_scene_info);
    Tasks::transmittance_lut_dispatch(*impl->device, impl->context, shaders_root);
    Tasks::multiscatter_lut_dispatch(*impl->device, impl->context, shaders_root);
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
    world_data.linear_sampler = impl->context.linear_sampler.id();
    world_data.nearest_sampler = impl->context.nearest_sampler.id();

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

auto WorldRenderer::begin_frame([[maybe_unused]] vuk::Value<vuk::ImageAttachment> &reference_img) -> void {
    ZoneScoped;

    impl->context.imgui_rendering_attachments.clear();
    impl->context.imgui_rendering_view_ids.clear();

    if (!impl->context.frame_resources) {
        impl->context.frame_resources = std::make_unique<FrameResources>(*impl->device, reference_img);
    }

    ImGui::NewFrame();
}

auto WorldRenderer::draw_scene() -> ls::pair<ImageViewID, vuk::Value<vuk::ImageAttachment>> {
    ZoneScoped;

    auto &final_image_view = impl->context.frame_resources->final_view;
    auto final_attachment = final_image_view.get_attachment(*impl->device, vuk::ImageUsageFlagBits::eColorAttachment);

    auto render_target = vuk::declare_ia("final_attachment", final_attachment);
    render_target = vuk::clear_image(std::move(render_target), vuk::Black<f32>);

    // render_target = Tasks::grid_draw(*impl->device, std::move(render_target), impl->context);

    auto sky_view = Tasks::sky_view_draw(*impl->device, impl->context);
    return { final_image_view.id(), std::move(sky_view) };
}

auto WorldRenderer::end_frame(vuk::Value<vuk::ImageAttachment> &&render_target) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    ImGui::Render();

    render_target = Tasks::imgui_draw(*impl->device, std::move(render_target), impl->context);

    return render_target;
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

auto WorldRenderer::composition_image_view() -> ImageView & {
    return impl->context.frame_resources->final_view;
}

auto WorldRenderer::imgui_image(ImageViewID image_view_id, vuk::Value<vuk::ImageAttachment> &&attachment, const glm::vec2 &size) -> void {
    ZoneScoped;

    impl->context.imgui_rendering_attachments.push_back(std::move(attachment));
    impl->context.imgui_rendering_view_ids.push_back(image_view_id);

    // make it +1 so it wont be nullptr, we already decrement it by one in imgui pass
    auto im_texture_id = reinterpret_cast<ImTextureID>(impl->context.imgui_rendering_attachments.size());
    ImGui::Image(im_texture_id, { size.x, size.y });
}

}  // namespace lr
