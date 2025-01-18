#pragma once

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
struct GPUSunData {
    glm::vec3 direction = {};
    f32 intensity = 10.0f;
};

struct GPUAtmosphereData {
    glm::vec3 rayleigh_scatter = { 0.005802f, 0.013558f, 0.033100f };
    f32 rayleigh_density = 8.0f;

    glm::vec3 mie_scatter = { 0.003996f, 0.003996f, 0.003996f };
    f32 mie_density = 1.2f;
    f32 mie_extinction = 0.004440f;
    f32 mie_asymmetry = 0.8f;

    glm::vec3 ozone_absorption = { 0.000650f, 0.001881f, 0.000085f };
    f32 ozone_height = 25.0f;
    f32 ozone_thickness = 15.0f;

    glm::vec3 terrain_albedo = { 0.3f, 0.3f, 0.3f };
    f32 aerial_km_per_slice = 8.0f;
    f32 planet_radius = 6360.0f;
    f32 atmos_radius = 6460.0f;
};

struct GPUClouds {
    glm::vec2 bounds = { 2.0f, 5.0f };
    f32 global_scale = 0.15f;
    f32 shape_noise_scale = 0.3f;
    glm::vec3 shape_noise_weights = { 0.625f, 0.25f, 0.125f };
    f32 detail_noise_scale = 0.3f;
    glm::vec3 detail_noise_weights = { 0.625f, 0.25f, 0.125f };
    f32 detail_noise_influence = 0.4f;
    f32 coverage = 0.9f;
    f32 general_density = 0.1f;
    glm::vec3 phase_values = { 0.623f, 0.335f, 0.979f };  // forwards phase, backwards phase, mix factor
    f32 cloud_type = 0.0f;
    f32 draw_distance = 1000.0f;
    f32 darkness_threshold = 0.450f;
    i32 sun_step_count = 5;
    i32 clouds_step_count = 30;
    f32 cloud_light_absorption = 0.443f;
    f32 sun_light_absorption = 0.224f;
};

struct GPUCameraData {
    glm::mat4 projection_mat = {};
    glm::mat4 view_mat = {};
    glm::mat4 projection_view_mat = {};
    glm::mat4 inv_view_mat = {};
    glm::mat4 inv_projection_view_mat = {};
    glm::vec3 position = {};
    f32 near_clip = {};
    f32 far_clip = {};
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

struct GPUModel {
    constexpr static auto MAX_MESHLET_INDICES = 64_sz;
    constexpr static auto MAX_MESHLET_PRIMITIVES = 64_sz;
    struct Vertex {
        glm::vec3 position = {};
        glm::vec3 normal = {};
        glm::vec2 tex_coord_0 = {};
        u32 color = 0;
    };
    using Index = u32;

    struct Meshlet {
        u32 vertex_offset = 0;
        u32 vertex_count = 0;
        u32 index_offset = 0;
        u32 index_count = 0;
    };

    struct Mesh {
        std::vector<Meshlet> meshlets = {};
    };

    std::vector<Mesh> meshes = {};

    u32 transform_index = 0;
    BufferID vertex_bufffer_id = BufferID::Invalid;
    BufferID index_buffer_id = BufferID::Invalid;
};

struct GPUModelTransformData {
    glm::mat4 model_transform_mat = {};
    glm::mat4 world_transform_mat = {};
};

// WARN: THIS IS GPU WORLD DATA, DO NOT CHANGE THIS WITHOUT
// SHADER MODIFICATION!!! PLACE CPU RELATED STUFF AT `WorldRenderContext`!
//
struct GPUWorldData {
    u32 linear_sampler_index = ~0_u32;
    u32 nearest_sampler_index = ~0_u32;

    u64 cameras_ptr = 0;
    u64 materials_ptr = 0;
    u64 model_transforms_ptr = 0;
    u64 sun_ptr = 0;
    u64 atmosphere_ptr = 0;
    u64 clouds_ptr = 0;

    u32 active_camera_index = 0;
    u32 padding = 0;

    auto rendering_atmos() -> bool { return sun_ptr != 0 && atmosphere_ptr != 0; }
};

struct SceneRenderBeginInfo {
    i32 camera_count = 0;
    i32 model_transform_count = 0;
    bool has_sun = false;
    bool has_atmosphere = false;
};

struct SceneRenderInfo {
    u32 active_camera_index = 0;
    BufferID materials_buffer_id = BufferID::Invalid;
    std::vector<GPUModel> models = {};

    GPUCameraData *cameras = nullptr;
    GPUModelTransformData *model_transforms = nullptr;
    GPUSunData *sun = nullptr;
    GPUAtmosphereData *atmosphere = nullptr;

    // Internals
    ls::option<u64> offset_cameras;
    ls::option<u64> offset_model_transforms;
    ls::option<u64> offset_models;
    ls::option<u64> offset_sun;
    ls::option<u64> offset_atmosphere;
    u64 upload_size = 0;
    TransientBuffer world_data = {};
};

struct SceneRenderer {
    auto init(this SceneRenderer &, Device *device) -> bool;
    auto setup_persistent_resources(this SceneRenderer &) -> void;

    auto begin_scene(this SceneRenderer &, const SceneRenderBeginInfo &info) -> SceneRenderInfo;
    auto end_scene(this SceneRenderer &, SceneRenderInfo &info) -> void;
    auto render(this SceneRenderer &, vuk::Format format, vuk::Extent3D extent) -> vuk::Value<vuk::ImageAttachment>;

    auto draw_profiler_ui(this SceneRenderer &) -> void;

private:
    Device *device = nullptr;

    u64 world_ptr = 0;
    GPUWorldData world_data = {};
    std::vector<GPUModel> rendering_models = {};

    Sampler linear_sampler = {};
    Sampler nearest_sampler = {};

    Pipeline grid_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Pipeline sky_transmittance_pipeline = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Pipeline sky_multiscatter_pipeline = {};
    Pipeline sky_view_pipeline = {};
    Pipeline sky_aerial_perspective_pipeline = {};
    Pipeline sky_final_pipeline = {};

    Pipeline vis_triangle_id_pipeline = {};

    Pipeline tonemap_pipeline = {};
};

}  // namespace lr
