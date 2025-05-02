#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct SceneComposeInfo {
    std::vector<ImageViewID> rendering_image_view_ids = {};
    std::vector<GPU::Material> gpu_materials = {};
    std::vector<GPU::Model> gpu_models = {};
    std::vector<GPU::MeshletInstance> gpu_meshlet_instances = {};
};

struct ComposedScene {
    vuk::Value<vuk::Buffer> materials_buffer = {};
    vuk::Value<vuk::Buffer> models_buffer = {};
    vuk::Value<vuk::Buffer> meshlet_instances_buffer = {};
};

struct SceneRenderInfo {
    vuk::Format format = vuk::Format::eR8G8B8A8Srgb;
    vuk::Extent3D extent = {};
    f32 delta_time = 0.0f;

    ls::option<GPU::Sun> sun = ls::nullopt;
    ls::option<GPU::Atmosphere> atmosphere = ls::nullopt;
    ls::option<GPU::Camera> camera = ls::nullopt;
    ls::option<glm::uvec2> picking_texel = ls::nullopt;
    ls::option<GPU::HistogramInfo> histogram_info = ls::nullopt;

    GPU::CullFlags cull_flags = {};
    ls::span<GPU::TransformID> dirty_transform_ids = {};
    ls::span<GPU::Transforms> transforms = {};

    ls::option<u32> picked_transform_index = ls::nullopt;
};

struct SceneRenderer {
    Device *device = nullptr;

    // Persistent resources
    vuk::PersistentDescriptorSet bindless_set = {};
    Sampler linear_repeat_sampler = {};
    Sampler linear_clamp_sampler = {};
    Sampler nearest_repeat_sampler = {};
    Buffer exposure_buffer = {};

    // Scene resources
    Buffer transforms_buffer = {};
    u32 meshlet_instance_count = 0;
    Buffer materials_buffer = {};
    Buffer models_buffer = {};
    Buffer meshlet_instances_buffer = {};

    // Then what are they?
    // TODO: Per scene sky settings
    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 312, .height = 192, .depth = 1 };
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };

    GPU::DebugView debug_view = GPU::DebugView::None;
    f32 debug_heatmap_scale = 5.0;

    auto init(this SceneRenderer &, Device *device) -> bool;
    auto destroy(this SceneRenderer &) -> void;

    auto create_persistent_resources(this SceneRenderer &) -> void;

    // Scene
    auto compose(this SceneRenderer &, SceneComposeInfo &compose_info) -> ComposedScene;
    auto cleanup(this SceneRenderer &) -> void;
    auto render(this SceneRenderer &, SceneRenderInfo &render_info, ls::option<ComposedScene> &composed_scene) -> vuk::Value<vuk::ImageAttachment>;
};

} // namespace lr
