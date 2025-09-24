#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct FramePrepareInfo {
    u32 image_count = 0;
    u32 mesh_instance_count = 0;
    u32 max_meshlet_instance_count = 0;
    bool regenerate_sky = false;

    ls::span<GPU::TransformID> dirty_transform_ids = {};
    ls::span<GPU::Transforms> gpu_transforms = {};

    ls::span<u32> dirty_material_indices = {};
    ls::span<GPU::Material> gpu_materials = {};

    ls::span<GPU::Mesh> gpu_meshes = {};
    ls::span<GPU::MeshInstance> gpu_mesh_instances = {};

    GPU::Camera camera = {};
    ls::option<GPU::DirectionalLight> directional_light = ls::nullopt;
    ls::span<glm::mat4> directional_light_clipmap_projection_view_mats = {};
    ls::option<GPU::Atmosphere> atmosphere = ls::nullopt;
    ls::option<GPU::EyeAdaptation> eye_adaptation = ls::nullopt;
    ls::option<GPU::VBGTAO> vbgtao = ls::nullopt;
};

struct PreparedFrame {
    u32 mesh_instance_count = 0;
    u32 max_meshlet_instance_count = 0;
    vuk::Value<vuk::Buffer> transforms_buffer = {};
    vuk::Value<vuk::Buffer> meshes_buffer = {};
    vuk::Value<vuk::Buffer> mesh_instances_buffer = {};
    vuk::Value<vuk::Buffer> meshlet_instance_visibility_mask_buffer = {};
    vuk::Value<vuk::Buffer> materials_buffer = {};
    vuk::Value<vuk::Buffer> camera_buffer = {};
    vuk::Value<vuk::Buffer> directional_light_buffer = {};
    vuk::Value<vuk::Buffer> directional_light_clipmaps_buffer = {};
    vuk::Value<vuk::Buffer> atmosphere_buffer = {};
    vuk::Value<vuk::Buffer> eye_adaptation_buffer = {};
    vuk::Value<vuk::Buffer> vbgtao_buffer = {};
    vuk::Value<vuk::ImageAttachment> sky_transmittance_lut = {};
    vuk::Value<vuk::ImageAttachment> sky_multiscatter_lut = {};
    vuk::Value<vuk::ImageAttachment> vsm_page_table = {};
    vuk::Value<vuk::ImageAttachment> vsm_physical_pages = {};

    bool has_atmosphere = false;
    bool has_eye_adaptation = false;
    bool has_vbgtao = false;
    GPU::Camera camera = {};
    ls::option<GPU::DirectionalLight> directional_light = ls::nullopt;
    glm::mat4 directional_light_clipmap_projection_view_mats[GPU::DirectionalLight::MAX_CLIPMAP_COUNT] = {};
};

struct SceneRenderInfo {
    f32 delta_time = 0.0f;
    u32 image_index = 0;
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
    Buffer meshlet_instance_visibility_mask_buffer = {};

    Buffer materials_buffer = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 192, .height = 108, .depth = 1 };
    vuk::Extent3D sky_cubemap_extent = { .width = 32, .height = 32, .depth = 1 };
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };

    Image hilbert_noise_lut = {};
    ImageView hilbert_noise_lut_view = {};

    // Many of the old papers related to virtual texturing
    // also calls this texture "indirection", same can be
    // said for VSM articles existed before Matej's
    Image vsm_page_table = {};
    ImageView vsm_page_table_view = {};
    Image vsm_physical_pages = {};
    ImageView vsm_physical_pages_view = {};

    bool debug_lines = false;
    f32 overdraw_heatmap_scale = 0.0f;
    u32 frame_counter = 0;

    auto init(this SceneRenderer &) -> bool;
    auto destroy(this SceneRenderer &) -> void;

    auto prepare_frame(this SceneRenderer &, FramePrepareInfo &info) -> PreparedFrame;
    auto render(this SceneRenderer &, vuk::Value<vuk::ImageAttachment> &&dst_attachment, SceneRenderInfo &render_info, PreparedFrame &frame)
        -> vuk::Value<vuk::ImageAttachment>;
    auto cleanup(this SceneRenderer &) -> void;
};

} // namespace lr
