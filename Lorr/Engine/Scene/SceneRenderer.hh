#pragma once

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
enum class GPUEntityID : u64 { Invalid = ~0_u64 };

struct GPUSunData {
    glm::vec3 direction = {};
    f32 intensity = 10.0f;
};

struct GPUAtmosphereData {
    alignas(4) glm::vec3 rayleigh_scatter = { 0.005802f, 0.013558f, 0.033100f };
    alignas(4) f32 rayleigh_density = 8.0f;

    alignas(4) glm::vec3 mie_scatter = { 0.003996f, 0.003996f, 0.003996f };
    alignas(4) f32 mie_density = 1.2f;
    alignas(4) f32 mie_extinction = 0.004440f;
    alignas(4) f32 mie_asymmetry = 0.8f;

    alignas(4) glm::vec3 ozone_absorption = { 0.000650f, 0.001881f, 0.000085f };
    alignas(4) f32 ozone_height = 25.0f;
    alignas(4) f32 ozone_thickness = 15.0f;

    alignas(4) glm::vec3 terrain_albedo = { 0.3f, 0.3f, 0.3f };
    alignas(4) f32 aerial_km_per_slice = 8.0f;
    alignas(4) f32 planet_radius = 6360.0f;
    alignas(4) f32 atmos_radius = 6460.0f;

    alignas(4) vuk::Extent3D transmittance_lut_size = {};
    alignas(4) vuk::Extent3D sky_view_lut_size = {};
    alignas(4) vuk::Extent3D multiscattering_lut_size = {};
    alignas(4) vuk::Extent3D aerial_perspective_lut_size = {};
};

struct GPUClouds {
    alignas(4) glm::vec2 bounds = { 2.0f, 5.0f };
    alignas(4) f32 global_scale = 0.15f;
    alignas(4) f32 shape_noise_scale = 0.3f;
    alignas(4) glm::vec3 shape_noise_weights = { 0.625f, 0.25f, 0.125f };
    alignas(4) f32 detail_noise_scale = 0.3f;
    alignas(4) glm::vec3 detail_noise_weights = { 0.625f, 0.25f, 0.125f };
    alignas(4) f32 detail_noise_influence = 0.4f;
    alignas(4) f32 coverage = 0.9f;
    alignas(4) f32 general_density = 0.1f;
    alignas(4) glm::vec3 phase_values = { 0.623f, 0.335f, 0.979f };  // forwards phase, backwards phase, mix factor
    alignas(4) f32 cloud_type = 0.0f;
    alignas(4) f32 draw_distance = 1000.0f;
    alignas(4) f32 darkness_threshold = 0.450f;
    alignas(4) i32 sun_step_count = 5;
    alignas(4) i32 clouds_step_count = 30;
    alignas(4) f32 cloud_light_absorption = 0.443f;
    alignas(4) f32 sun_light_absorption = 0.224f;
};

struct GPUCameraData {
    alignas(4) glm::mat4 projection_mat = {};
    alignas(4) glm::mat4 view_mat = {};
    alignas(4) glm::mat4 projection_view_mat = {};
    alignas(4) glm::mat4 inv_view_mat = {};
    alignas(4) glm::mat4 inv_projection_view_mat = {};
    alignas(4) glm::vec3 position = {};
    alignas(4) f32 near_clip = {};
    alignas(4) f32 far_clip = {};
};

struct GPUMaterial {
    alignas(4) glm::vec3 albedo_color = { 1.0f, 1.0f, 1.0f };
    alignas(4) glm::vec3 emissive_color = { 0.0f, 0.0f, 0.0f };
    alignas(4) f32 roughness_factor = 0.0f;
    alignas(4) f32 metallic_factor = 0.0f;
    alignas(4) u32 alpha_mode = 0;
    alignas(4) f32 alpha_cutoff = 0.0f;
    alignas(4) SampledImage albedo_image = {};
    alignas(4) SampledImage normal_image = {};
    alignas(4) SampledImage emissive_image = {};
};

struct GPUMeshInstance {
    alignas(4) u32 entity_index = ~0_u32;
    alignas(4) u32 material_index = ~0_u32;
};

struct GPUEntityTransform {
    alignas(4) glm::mat4 local_transform_mat = {};
};

struct RenderingMesh {
    u32 entity_index = ~0_u32;
    u32 vertex_offset = 0;
    u32 index_count = 0;
    u32 index_offset = 0;

    // TODO: Remove them when we have mega-buffers
    BufferID vertex_buffer_id = BufferID::Invalid;
    BufferID reordered_index_buffer_id = BufferID::Invalid;
    BufferID provoked_index_buffer_id = BufferID::Invalid;
};

struct SceneRenderInfo {
    vuk::Format format = vuk::Format::eR8G8B8A8Srgb;
    vuk::Extent3D extent = {};

    BufferID materials_buffer_id = BufferID::Invalid;
    std::vector<RenderingMesh> rendering_meshes = {};

    ls::option<GPUCameraData> camera_info = ls::nullopt;
    ls::option<GPUSunData> sun = ls::nullopt;
    ls::option<GPUAtmosphereData> atmosphere = ls::nullopt;
};

struct SceneRenderer {
    auto init(this SceneRenderer &, Device *device) -> bool;
    auto setup_persistent_resources(this SceneRenderer &) -> void;

    auto update_entity_transform(this SceneRenderer &, GPUEntityID entity_id, GPUEntityTransform transform) -> void;

    auto render(this SceneRenderer &, const SceneRenderInfo &render_info) -> vuk::Value<vuk::ImageAttachment>;
    auto draw_profiler_ui(this SceneRenderer &) -> void;

private:
    Device *device = nullptr;
    Buffer gpu_entities_buffer = {};

    Pipeline grid_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Pipeline sky_transmittance_pipeline = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Pipeline sky_multiscatter_pipeline = {};
    Pipeline sky_view_pipeline = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 208, .height = 128, .depth = 1 };
    Pipeline sky_aerial_perspective_pipeline = {};
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };
    Pipeline sky_final_pipeline = {};

    Pipeline vis_triangle_id_pipeline = {};

    Pipeline tonemap_pipeline = {};
};

}  // namespace lr
