#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct FramePrepareInfo {
    u32 mesh_instance_count = 0;
    u32 max_meshlet_instance_count = 0;
    bool regenerate_sky = false;

    ls::span<GPU::TransformID> dirty_transform_ids = {};
    ls::span<GPU::Transforms> gpu_transforms = {};

    ls::span<u32> dirty_material_indices = {};
    ls::span<GPU::Material> gpu_materials = {};

    ls::span<GPU::Mesh> gpu_meshes = {};
    ls::span<GPU::MeshInstance> gpu_mesh_instances = {};

    GPU::Environment environment = {};
    GPU::Camera camera = {};
};

struct PreparedFrame {
    u32 mesh_instance_count = 0;
    u32 max_meshlet_instance_count = 0;
    GPU::EnvironmentFlags environment_flags = GPU::EnvironmentFlags::None;
    vuk::Value<vuk::Buffer> transforms_buffer = {};
    vuk::Value<vuk::Buffer> meshes_buffer = {};
    vuk::Value<vuk::Buffer> mesh_instances_buffer = {};
    vuk::Value<vuk::Buffer> mesh_instance_visibility_mask_buffer = {};
    vuk::Value<vuk::Buffer> meshlet_instance_visibility_mask_buffer = {};
    vuk::Value<vuk::Buffer> materials_buffer = {};
    vuk::Value<vuk::Buffer> environment_buffer = {};
    vuk::Value<vuk::Buffer> camera_buffer = {};
    vuk::Value<vuk::ImageAttachment> sky_transmittance_lut = {};
    vuk::Value<vuk::ImageAttachment> sky_multiscatter_lut = {};
};

struct SceneRenderInfo {
    f32 delta_time = 0.0f;
    GPU::CullFlags cull_flags = {};

    ls::option<glm::uvec2> picking_texel = ls::nullopt;
    ls::option<u32> picked_transform_index = ls::nullopt;
};

struct SceneRenderer {
    static constexpr auto MODULE_NAME = "Scene Renderer";

    // Scene resources
    Buffer histogram_luminance_buffer = {};
    Buffer transforms_buffer = {};

    Buffer mesh_instances_buffer = {};
    Buffer meshes_buffer = {};
    Buffer mesh_instance_visibility_mask_buffer = {};
    Buffer meshlet_instance_visibility_mask_buffer = {};

    Buffer materials_buffer = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 312, .height = 192, .depth = 1 };
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };

    Image hiz = {};
    ImageView hiz_view = {};

    bool debug_lines = false;
    f32 overdraw_heatmap_scale = 0.0f;

    auto init(this SceneRenderer &) -> bool;
    auto destroy(this SceneRenderer &) -> void;

    auto prepare_frame(this SceneRenderer &, FramePrepareInfo &info) -> PreparedFrame;
    auto render(this SceneRenderer &, vuk::Value<vuk::ImageAttachment> &&dst_attachment, SceneRenderInfo &render_info, PreparedFrame &frame)
        -> vuk::Value<vuk::ImageAttachment>;
    auto cleanup(this SceneRenderer &) -> void;
};

} // namespace lr
