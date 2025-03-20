#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr::GPU {
enum class TransformID : u64 { Invalid = ~0_u64 };
struct Transforms {
    alignas(4) glm::mat4 local = {};
    alignas(4) glm::mat4 world = {};
    alignas(4) glm::mat3 normal = {};
};

struct Sun {
    alignas(4) glm::vec3 direction = {};
    alignas(4) f32 intensity = 10.0f;
};

struct Atmosphere {
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
    alignas(4) f32 aerial_gain_per_slice = 8.0f;
    alignas(4) f32 planet_radius = 6360.0f;
    alignas(4) f32 atmos_radius = 6460.0f;

    alignas(4) vuk::Extent3D transmittance_lut_size = {};
    alignas(4) vuk::Extent3D sky_view_lut_size = {};
    alignas(4) vuk::Extent3D multiscattering_lut_size = {};
    alignas(4) vuk::Extent3D aerial_perspective_lut_size = {};
};

struct Clouds {
    alignas(4) glm::vec2 bounds = { 1.0f, 10.0f };
    alignas(4) glm::vec4 shape_noise_weights = { 0.625f, 0.25f, 0.15f, 0.625f };
    alignas(4) glm::vec4 detail_noise_weights = { 0.625f, 0.25f, 0.15f, 0.625f };
    alignas(4) f32 shape_noise_scale = 100.0f;
    alignas(4) f32 detail_noise_scale = 100.0f;
    alignas(4) f32 detail_noise_influence = 0.4f;
    alignas(4) f32 coverage = 0.5f;
    alignas(4) f32 general_density = 4.0f;
    // forwards phase, backwards phase, forwards peak mix factor
    alignas(4) glm::vec3 phase_values = { 0.427f, -0.335f, 0.15f };
    alignas(4) f32 cloud_type = 0.0f;
    alignas(4) f32 draw_distance = 5000.0f;
    alignas(4) f32 darkness_threshold = 0.02f;
    alignas(4) i32 sun_step_count = 5;
    alignas(4) i32 clouds_step_count = 64;
    alignas(4) f32 extinction = 0.22f;
    alignas(4) f32 scattering = 0.16f;
    alignas(4) f32 powder_intensity = 10.0;

    alignas(4) vuk::Extent3D noise_lut_size = {};
};

struct Camera {
    alignas(4) glm::mat4 projection_mat = {};
    alignas(4) glm::mat4 view_mat = {};
    alignas(4) glm::mat4 projection_view_mat = {};
    alignas(4) glm::mat4 inv_view_mat = {};
    alignas(4) glm::mat4 inv_projection_view_mat = {};
    alignas(4) glm::vec3 position = {};
    alignas(4) f32 near_clip = {};
    alignas(4) f32 far_clip = {};
};

struct Scene {
    alignas(4) Camera camera = {};
    alignas(4) Sun sun = {};
    alignas(4) Atmosphere atmosphere = {};
    alignas(4) Clouds clouds = {};
    alignas(4) u32 meshlet_instance_count = 0;
};

struct Material {
    alignas(4) glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    alignas(4) glm::vec3 emissive_color = { 0.0f, 0.0f, 0.0f };
    alignas(4) f32 roughness_factor = 0.0f;
    alignas(4) f32 metallic_factor = 0.0f;
    alignas(4) u32 alpha_mode = 0;
    alignas(4) f32 alpha_cutoff = 0.0f;
    alignas(4) u32 albedo_image_index = 0;
    alignas(4) u32 normal_image_index = 0;
    alignas(4) u32 emissive_image_index = 0;
    alignas(4) u32 metallic_roughness_image_index = 0;
    alignas(4) u32 occlusion_image_index = 0;
};

struct Meshlet {
    alignas(4) u32 vertex_offset = 0;
    alignas(4) u32 index_offset = 0;
    alignas(4) u32 triangle_offset = 0;
    alignas(4) u32 triangle_count = 0;
};

struct MeshletBounds {
    alignas(4) glm::vec3 aabb_min = {};
    alignas(4) glm::vec3 aabb_max = {};
};

struct MeshletInstance {
    alignas(4) u32 model_index = 0;
    alignas(4) u32 material_index = 0;
    alignas(4) u32 transform_index = 0;
    alignas(4) u32 meshlet_index = 0;
};

struct Model {
    alignas(8) u64 indices = 0;
    alignas(8) u64 vertex_positions = 0;
    alignas(8) u64 vertex_normals = 0;
    alignas(8) u64 texture_coords = 0;
    alignas(8) u64 meshlets = 0;
    alignas(8) u64 meshlet_bounds = 0;
    alignas(8) u64 local_triangle_indices = 0;
};
} // namespace lr::GPU
