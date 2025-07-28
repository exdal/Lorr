#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct SceneComposeInfo {
    std::vector<GPU::Mesh> gpu_meshes = {};
    std::vector<GPU::MeshInstance> gpu_mesh_instances = {};
    std::vector<GPU::MeshletInstance> gpu_meshlet_instances = {};
};

struct ComposedScene {
    vuk::Value<vuk::Buffer> meshlet_instances_buffer = {};
    vuk::Value<vuk::Buffer> mesh_instances_buffer = {};
    vuk::Value<vuk::Buffer> meshes_buffer = {};
};

struct SceneRenderInfo {
    vuk::Format format = vuk::Format::eR8G8B8A8Srgb;
    vuk::Extent3D extent = {};
    f32 delta_time = 0.0f;

    vuk::PersistentDescriptorSet *materials_descriptor_set = nullptr;
    vuk::Value<vuk::Buffer> materials_buffer = {};

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

    // Scene resources
    Buffer exposure_buffer = {};
    Buffer transforms_buffer = {};
    u32 meshlet_instance_count = 0;
    Buffer meshlet_instances_buffer = {};
    u32 mesh_instance_count = 0;
    Buffer mesh_instances_buffer = {};
    Buffer meshes_buffer = {};

    // Then what are they?
    // TODO: Per scene sky settings
    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 312, .height = 192, .depth = 1 };
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };

    Image hiz = {};
    ImageView hiz_view = {};

    bool debug_lines = false;
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
