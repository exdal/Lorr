#include "Engine/World/WorldRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/World/Tasks/ImGui.hh"

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
    transfer_man.upload_staging(impl->context.imgui_font_view, imgui_atlas_data);
    impl->context.imgui_pipeline = Pipeline::create(*impl->device, {
        .module_name = "imgui",
        .root_path = shaders_root,
        .shader_path = shaders_root / "imgui.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    // clang-format on
}

auto WorldRenderer::set_scene_data(WorldRenderer::SceneBeginInfo &info) -> void {
    ZoneScoped;
}

auto WorldRenderer::update_world_data() -> void {
    ZoneScoped;

    auto &context = impl->context;
    auto &world_data = context.world_data;
}

auto WorldRenderer::render(vuk::Value<vuk::ImageAttachment> &&target_attachment) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto cleared_image = vuk::clear_image(std::move(target_attachment), vuk::Black<f32>);
    auto imgui_image = Tasks::imgui_draw(*impl->device, std::move(cleared_image), impl->context);

    return imgui_image;
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

auto WorldRenderer::composition_image() -> Image {
    return Image{};
}

}  // namespace lr
