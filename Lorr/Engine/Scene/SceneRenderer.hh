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

    ls::option<GPU::Sun> sun = ls::nullopt;
    ls::option<GPU::Atmosphere> atmosphere = ls::nullopt;
    ls::option<GPU::Camera> camera = ls::nullopt;
    ls::option<glm::uvec2> picking_texel = ls::nullopt;
    GPU::CullFlags cull_flags = {};
    ls::span<GPU::TransformID> dirty_transform_ids = {};
    ls::span<GPU::Transforms> transforms = {};

    ls::option<u32> picked_transform_index = ls::nullopt;
};

struct SceneRenderer {
    Device *device = nullptr;

    vuk::PersistentDescriptorSet bindless_set = {};
    Sampler linear_repeat_sampler = {};
    Sampler linear_clamp_sampler = {};
    Sampler nearest_repeat_sampler = {};

    Buffer transforms_buffer = {};
    u32 meshlet_instance_count = 0;
    Buffer materials_buffer = {};
    Buffer models_buffer = {};
    Buffer meshlet_instances_buffer = {};

    Pipeline editor_grid_pipeline = {};
    Pipeline editor_mousepick_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Pipeline sky_transmittance_pipeline = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Pipeline sky_multiscatter_pipeline = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 208, .height = 128, .depth = 1 };
    Pipeline sky_view_pipeline = {};
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };
    Pipeline sky_aerial_perspective_pipeline = {};
    Pipeline sky_final_pipeline = {};

    Pipeline vis_cull_meshlets_pipeline = {};
    Pipeline vis_cull_triangles_pipeline = {};
    Pipeline vis_encode_pipeline = {};
    Pipeline vis_decode_pipeline = {};

    Pipeline pbr_basic_pipeline = {};

    Pipeline tonemap_pipeline = {};

    auto init(this SceneRenderer &, Device *device) -> bool;
    auto destroy(this SceneRenderer &) -> void;

    auto create_persistent_resources(this SceneRenderer &) -> void;

    // Scene
    auto compose(this SceneRenderer &, SceneComposeInfo &compose_info) -> ComposedScene;
    auto render(this SceneRenderer &, SceneRenderInfo &render_info, ls::option<ComposedScene> &composed_scene) -> vuk::Value<vuk::ImageAttachment>;
};

} // namespace lr
