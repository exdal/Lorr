#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr::GPU {
enum class DebugView : i32 {
    None = 0,
    Triangles,
    Meshlets,
    Overdraw,
    Albedo,
    Normal,
    Emissive,
    Metallic,
    Roughness,
    Occlusion,

    Count,
};

struct DrawIndirectCommand {
    u32 vertex_count = 0;
    u32 instance_count = 0;
    u32 first_vertex = 0;
    u32 first_instance = 0;
};

struct DebugDrawData {
    u32 draw_count = 0;
    u32 capacity = 0;
};

struct DebugAABB {
    glm::vec3 position = {};
    glm::vec3 size = {};
    glm::vec3 color = {};
    bool is_ndc = false;
};

struct DebugRect {
    glm::vec3 offset = {};
    glm::vec2 extent = {};
    glm::vec3 color = {};
    bool is_ndc = false;
};

struct DebugDrawer {
    DrawIndirectCommand aabb_draw_cmd = {};
    DebugDrawData aabb_data = {};
    u64 aabb_buffer = 0;

    DrawIndirectCommand rect_draw_cmd = {};
    DebugDrawData rect_data = {};
    u64 rect_buffer = 0;
};

enum class CullFlags : u32 {
    MeshletFrustum = 1 << 0,
    TriangleBackFace = 1 << 1,
    MicroTriangles = 1 << 2,
    Occlusion = 1 << 3,

    All = MeshletFrustum | TriangleBackFace | MicroTriangles | Occlusion,
};

enum EnvironmentFlags : u32 {
    None = 0,
    HasSun = 1 << 0,
    HasAtmosphere = 1 << 1,
    HasEyeAdaptation = 1 << 2,
};

struct Environment {
    alignas(4) u32 flags = EnvironmentFlags::None;
    // Sun
    alignas(4) glm::vec3 sun_direction = {};
    alignas(4) f32 sun_intensity = 10.0f;
    // Atmosphere
    alignas(4) glm::vec3 atmos_rayleigh_scatter = { 0.005802f, 0.014338f, 0.032800f };
    alignas(4) f32 atmos_rayleigh_density = 8.0f;
    alignas(4) glm::vec3 atmos_mie_scatter = { 0.003996f, 0.003996f, 0.003996f };
    alignas(4) f32 atmos_mie_density = 1.2f;
    alignas(4) f32 atmos_mie_extinction = 0.004440f;
    alignas(4) f32 atmos_mie_asymmetry = 3.5f;
    alignas(4) glm::vec3 atmos_ozone_absorption = { 0.000650f, 0.001781f, 0.000085f };
    alignas(4) f32 atmos_ozone_height = 25.0f;
    alignas(4) f32 atmos_ozone_thickness = 15.0f;
    alignas(4) glm::vec3 atmos_terrain_albedo = { 0.3f, 0.3f, 0.3f };
    alignas(4) f32 atmos_planet_radius = 6360.0f;
    alignas(4) f32 atmos_atmos_radius = 6460.0f;
    alignas(4) f32 atmos_aerial_perspective_start_km = 8.0f;
    // Eye adaptation
    alignas(4) f32 eye_min_exposure = -6.0f;
    alignas(4) f32 eye_max_exposure = 18.0f;
    alignas(4) f32 eye_adaptation_speed = 1.1f;
    alignas(4) f32 eye_ISO_K = 100.0f / 12.5f;

    alignas(4) vuk::Extent3D transmittance_lut_size = {};
    alignas(4) vuk::Extent3D sky_view_lut_size = {};
    alignas(4) vuk::Extent3D multiscattering_lut_size = {};
    alignas(4) vuk::Extent3D aerial_perspective_lut_size = {};
};

constexpr static f32 CAMERA_SCALE_UNIT = 0.01;
constexpr static f32 INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
constexpr static f32 PLANET_RADIUS_OFFSET = 0.001;

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
    alignas(4) glm::vec2 resolution = {};
    alignas(4) f32 acceptable_lod_error = 0.0f;
};

enum class TransformID : u64 { Invalid = ~0_u64 };
struct Transforms {
    alignas(4) glm::mat4 local = {};
    alignas(4) glm::mat4 world = {};
    alignas(4) glm::mat3 normal = {};
};

enum class MaterialFlag : u32 {
    None = 0,
    // Image flags
    HasAlbedoImage = 1 << 0,
    HasNormalImage = 1 << 1,
    HasEmissiveImage = 1 << 2,
    HasMetallicRoughnessImage = 1 << 3,
    HasOcclusionImage = 1 << 4,
    // Normal flags
    NormalTwoComponent = 1 << 5,
    NormalFlipY = 1 << 6,
    // Alpha
    AlphaOpaque = 1 << 7,
    AlphaMask = 1 << 8,
    AlphaBlend = 1 << 9,
};
consteval void enable_bitmask(MaterialFlag);

struct Material {
    alignas(4) glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    alignas(4) glm::vec3 emissive_color = { 0.0f, 0.0f, 0.0f };
    alignas(4) f32 roughness_factor = 0.0f;
    alignas(4) f32 metallic_factor = 0.0f;
    alignas(4) f32 alpha_cutoff = 0.0f;
    alignas(4) MaterialFlag flags = MaterialFlag::None;
    alignas(4) u32 sampler_index = 0;
    alignas(4) u32 albedo_image_index = 0;
    alignas(4) u32 normal_image_index = 0;
    alignas(4) u32 emissive_image_index = 0;
    alignas(4) u32 metallic_roughness_image_index = 0;
    alignas(4) u32 occlusion_image_index = 0;
};

struct Bounds {
    alignas(4) glm::vec3 aabb_center = {};
    alignas(4) glm::vec3 aabb_extent = {};
    alignas(4) glm::vec3 sphere_center = {};
    alignas(4) f32 sphere_radius = 0.0f;
};

struct MeshletInstance {
    alignas(4) u32 mesh_instance_index = 0;
    alignas(4) u32 meshlet_index = 0;
};

struct MeshInstance {
    alignas(4) u32 mesh_index = 0;
    alignas(4) u32 lod_index = 0;
    alignas(4) u32 material_index = 0;
    alignas(4) u32 transform_index = 0;
};

struct Meshlet {
    alignas(4) u32 indirect_vertex_index_offset = 0;
    alignas(4) u32 local_triangle_index_offset = 0;
    alignas(4) u32 vertex_count = 0;
    alignas(4) u32 triangle_count = 0;
};

struct MeshLOD {
    alignas(8) u64 indices = 0;
    alignas(8) u64 meshlets = 0;
    alignas(8) u64 meshlet_bounds = 0;
    alignas(8) u64 local_triangle_indices = 0;
    alignas(8) u64 indirect_vertex_indices = 0;

    alignas(4) u32 indices_count = 0;
    alignas(4) u32 meshlet_count = 0;
    alignas(4) u32 meshlet_bounds_count = 0;
    alignas(4) u32 local_triangle_indices_count = 0;
    alignas(4) u32 indirect_vertex_indices_count = 0;

    alignas(4) f32 error = 0.0f;
};

struct Mesh {
    constexpr static auto MAX_LODS = 8_sz;

    alignas(8) u64 vertex_positions = 0;
    alignas(8) u64 vertex_normals = 0;
    alignas(8) u64 texture_coords = 0;
    alignas(4) u32 _padding = 0;
    alignas(4) u32 lod_count = 0;
    alignas(8) MeshLOD lods[MAX_LODS] = {};
    alignas(4) Bounds bounds = {};
};

constexpr static u32 HISTOGRAM_THREADS_X = 16;
constexpr static u32 HISTOGRAM_THREADS_Y = 16;
constexpr static u32 HISTOGRAM_BIN_COUNT = HISTOGRAM_THREADS_X * HISTOGRAM_THREADS_Y;

struct HistogramLuminance {
    f32 adapted_luminance = 0.0f;
    f32 exposure = 0.0f;
};

} // namespace lr::GPU
