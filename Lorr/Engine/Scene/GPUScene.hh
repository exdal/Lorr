#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr::GPU {
enum class CullFlags : u32 {
    MeshletFrustum = 1 << 0,
    TriangleBackFace = 1 << 1,
    MicroTriangles = 1 << 2,

    All = MeshletFrustum | TriangleBackFace | MicroTriangles,
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

struct Camera {
    alignas(4) glm::mat4 projection_mat = {};
    alignas(4) glm::mat4 view_mat = {};
    alignas(4) glm::mat4 projection_view_mat = {};
    alignas(4) glm::mat4 inv_view_mat = {};
    alignas(4) glm::mat4 inv_projection_view_mat = {};
    alignas(4) glm::mat4 frustum_projection_view_mat = {};
    alignas(4) glm::vec3 position = {};
    alignas(4) f32 near_clip = {};
    alignas(4) f32 far_clip = {};
    alignas(4) glm::vec4 frustum_planes[6] = {};
    alignas(4) glm::vec2 resolution = {};
};

enum class TransformID : u64 { Invalid = ~0_u64 };
struct Transforms {
    alignas(4) glm::mat4 local = {};
    alignas(4) glm::mat4 world = {};
    alignas(4) glm::mat3 normal = {};
};

enum class AlphaMode : u32 {
    Opaque = 0,
    Mask,
    Blend,
};

struct Material {
    alignas(4) glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    alignas(4) glm::vec3 emissive_color = { 0.0f, 0.0f, 0.0f };
    alignas(4) f32 roughness_factor = 0.0f;
    alignas(4) f32 metallic_factor = 0.0f;
    alignas(4) AlphaMode alpha_mode = AlphaMode::Opaque;
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
