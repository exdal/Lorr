#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

#include <bit>

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
    MeshFrustum = 1 << 0,
    MeshOcclusion = 1 << 1,
    MeshletFrustum = 1 << 2,
    MeshletOcclusion = 1 << 3,
    TriangleBackFace = 1 << 4,
    MicroTriangles = 1 << 5,

    All = ~0_u32,
};

constexpr static f32 CAMERA_SCALE_UNIT = 0.01;
constexpr static f32 INV_CAMERA_SCALE_UNIT = 1.0 / CAMERA_SCALE_UNIT;
constexpr static f32 PLANET_RADIUS_OFFSET = 0.001;

struct Camera {
    alignas(4) glm::mat4 projection_mat = {};
    alignas(4) glm::mat4 inv_projection_mat = {};
    alignas(4) glm::mat4 view_mat = {};
    alignas(4) glm::mat4 inv_view_mat = {};
    alignas(4) glm::mat4 projection_view_mat = {};
    alignas(4) glm::mat4 inv_projection_view_mat = {};
    alignas(4) glm::vec3 position = {};
    alignas(4) glm::vec2 resolution = {};
    alignas(4) f32 near_clip = 0.01f;
    alignas(4) f32 far_clip = 1000.0f;
    alignas(4) f32 acceptable_lod_error = 0.0f;
    alignas(4) f32 fov_deg = 65.0f;
    alignas(4) f32 aspect_ratio = 1.777f;
};

constexpr static u32 VSM_PAGE_SIZE = 128;
constexpr static u32 VSM_PAGE_COUNT = 1024;
constexpr static u32 VSM_MIN_VIRTUAL_EXTENT = VSM_PAGE_SIZE;
constexpr static u32 VSM_MAX_VIRTUAL_EXTENT = 4096;
constexpr static u32 VSM_PAGE_TABLE_SIZE = VSM_MAX_VIRTUAL_EXTENT / VSM_PAGE_SIZE;
constexpr static u32 VSM_PAGE_TABLE_MIP_COUNT = std::bit_width(VSM_PAGE_TABLE_SIZE);

struct DirectionalLight {
    constexpr static auto MAX_CLIPMAP_COUNT = 32_u32;

    alignas(4) glm::vec3 base_ambient_color = {};
    alignas(4) f32 intensity = {};
    alignas(4) glm::vec3 direction = {};
    alignas(4) u32 clipmap_count = {};
    alignas(4) f32 depth_bias = {};
    alignas(4) f32 normal_bias = {};
};

struct Lights {
    alignas(8) u64 directional_light = 0;
    alignas(8) u64 directional_light_clipmap_projection_views = 0;
};

struct Atmosphere {
    alignas(4) glm::vec3 sun_direction = {};
    alignas(4) f32 eye_height = {};
    alignas(4) glm::vec3 rayleigh_scatter = { 0.005802f, 0.014338f, 0.032800f };
    alignas(4) f32 rayleigh_density = 8.0f;
    alignas(4) glm::vec3 mie_scatter = { 0.003996f, 0.003996f, 0.003996f };
    alignas(4) f32 mie_density = 1.2f;
    alignas(4) f32 mie_extinction = 0.004440f;
    alignas(4) f32 mie_asymmetry = 3.5f;
    alignas(4) glm::vec3 ozone_absorption = { 0.000650f, 0.001781f, 0.000085f };
    alignas(4) f32 ozone_height = 25.0f;
    alignas(4) f32 ozone_thickness = 15.0f;
    alignas(4) glm::vec3 terrain_albedo = { 0.3f, 0.3f, 0.3f };
    alignas(4) f32 planet_radius = 6360.0f;
    alignas(4) f32 atmos_radius = 6460.0f;
    alignas(4) f32 aerial_perspective_start_km = 8.0f;
    alignas(4) f32 sun_intensity = {};

    alignas(4) vuk::Extent3D transmittance_lut_size = {};
    alignas(4) vuk::Extent3D sky_view_lut_size = {};
    alignas(4) vuk::Extent3D multiscattering_lut_size = {};
    alignas(4) vuk::Extent3D aerial_perspective_lut_size = {};
};

struct PBRContext {
    alignas(8) u64 atmosphere = 0;
    alignas(8) u64 directional_light = 0;
    alignas(8) u64 directional_light_clipmaps = 0;
};

struct EyeAdaptation {
    alignas(4) f32 min_exposure = -6.0f;
    alignas(4) f32 max_exposure = 18.0f;
    alignas(4) f32 adaptation_speed = 1.1f;
    alignas(4) f32 ISO_K = 100.0f / 12.5f;
};

struct VBGTAO {
    alignas(4) f32 thickness = 0.25f;
    alignas(4) f32 depth_range_scale_factor = 0.75f;
    alignas(4) f32 radius = 0.5f;
    alignas(4) f32 radius_multiplier = 1.457f;
    alignas(4) f32 slice_count = 3.0f;
    alignas(4) f32 sample_count_per_slice = 3.0f;
    alignas(4) f32 denoise_power = 1.1f;
    alignas(4) f32 linear_thickness_multiplier = 300.0f;
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
    alignas(4) u32 meshlet_instance_visibility_offset = 0;
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
    alignas(4) u32 vertex_count = 0;
    alignas(4) u32 lod_count = 0;
    alignas(8) MeshLOD lods[MAX_LODS] = {};
    alignas(4) Bounds bounds = {};
};

constexpr static u32 HISTOGRAM_THREADS_X = 16;
constexpr static u32 HISTOGRAM_THREADS_Y = 16;
constexpr static u32 HISTOGRAM_BIN_COUNT = HISTOGRAM_THREADS_X * HISTOGRAM_THREADS_Y;

struct HistogramLuminance {
    alignas(4) f32 adapted_luminance = 0.0f;
    alignas(4) f32 exposure = 0.0f;
};

} // namespace lr::GPU
