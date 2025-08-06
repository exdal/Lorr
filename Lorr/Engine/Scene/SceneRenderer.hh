#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct FramePrepareInfo {
    u32 mesh_instance_count = 0;
    u32 meshlet_instance_count = 0;

    ls::span<u32> dirty_texture_indices = {};
    ls::span<ls::pair<ImageViewID, SamplerID>> dirty_textures = {};

    ls::span<GPU::TransformID> dirty_transform_ids = {};
    ls::span<GPU::Transforms> gpu_transforms = {};

    ls::span<u32> dirty_material_indices = {};
    ls::span<GPU::Material> gpu_materials = {};

    ls::span<GPU::Mesh> gpu_meshes = {};
    ls::span<GPU::MeshInstance> gpu_mesh_instances = {};
    ls::span<GPU::MeshletInstance> gpu_meshlet_instances = {};
};

struct PreparedFrame {
    vuk::Value<vuk::Buffer> transforms_buffer = {};
    vuk::Value<vuk::Buffer> meshes_buffer = {};
    vuk::Value<vuk::Buffer> mesh_instances_buffer = {};
    vuk::Value<vuk::Buffer> meshlet_instances_buffer = {};
    vuk::Value<vuk::Buffer> materials_buffer = {};
};

struct SceneRenderInfo {
    vuk::Format format = vuk::Format::eR8G8B8A8Srgb;
    vuk::Extent3D extent = {};
    f32 delta_time = 0.0f;
    GPU::CullFlags cull_flags = {};

    ls::option<GPU::Sun> sun = ls::nullopt;
    ls::option<GPU::Atmosphere> atmosphere = ls::nullopt;
    ls::option<GPU::Camera> camera = ls::nullopt;
    ls::option<glm::uvec2> picking_texel = ls::nullopt;
    ls::option<GPU::HistogramInfo> histogram_info = ls::nullopt;

    ls::option<u32> picked_transform_index = ls::nullopt;
};

struct SceneRenderer {
    Device *device = nullptr;

    // Scene resources
    Buffer exposure_buffer = {};
    Buffer transforms_buffer = {};

    u32 mesh_instance_count = 0;
    u32 meshlet_instance_count = 0;
    Buffer mesh_instances_buffer = {};
    Buffer meshes_buffer = {};
    Buffer meshlet_instances_buffer = {};

    vuk::PersistentDescriptorSet materials_descriptor_set = {};
    Buffer materials_buffer = {};

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

    auto init(this SceneRenderer &, Device *device) -> bool;
    auto destroy(this SceneRenderer &) -> void;

    auto create_persistent_resources(this SceneRenderer &) -> void;

    // Scene
    auto prepare_frame(this SceneRenderer &, FramePrepareInfo &info) -> PreparedFrame;
    auto render(this SceneRenderer &, SceneRenderInfo &render_info, PreparedFrame &frame) -> vuk::Value<vuk::ImageAttachment>;
    auto cleanup(this SceneRenderer &) -> void;
};

} // namespace lr
