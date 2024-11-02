#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
struct GPUSunData {
    glm::vec3 direction = {};
    f32 intensity = 10.0f;
};

struct GPUAtmosphereData {
    glm::vec3 rayleigh_scatter = glm::vec3(5.802, 13.558, 33.1) * 1e-6f;
    f32 rayleigh_density = 8.0 * 1e3f;
    f32 planet_radius = 6360.0 * 1e3f;
    f32 atmos_radius = 6460.0 * 1e3f;
    f32 mie_scatter = 3.996 * 1e-6f;
    f32 mie_absorption = 4.4 * 1e-6f;
    f32 mie_density = 1.2 * 1e3f;
    f32 mie_asymmetry = 0.8;
    f32 ozone_height = 25 * 1e3f;
    f32 ozone_thickness = 15 * 1e3f;
    glm::vec3 ozone_absorption = glm::vec3(0.650, 1.881, 0.085) * 1e-6f;
    f32 _unused;
};

enum class GPUMaterialID : u32 { Invalid = ~0_u32 };
struct GPUMaterial {
    glm::vec4 albedo_color = { 1.0f, 1.0f, 1.0f, 1.0f };
    glm::vec4 emissive_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    f32 roughness_factor = 0.0f;
    f32 metallic_factor = 0.0f;
    vk::AlphaMode alpha_mode = vk::AlphaMode::Opaque;
    f32 alpha_cutoff = 0.0f;
    ImageID albedo_image = ImageID::Invalid;
    SamplerID albedo_sampler = SamplerID::Invalid;
    ImageID normal_image = ImageID::Invalid;
    SamplerID normal_sampler = SamplerID::Invalid;
    ImageID emissive_image = ImageID::Invalid;
    SamplerID emissive_sampler = SamplerID::Invalid;
};

struct GPUMeshPrimitive {
    u32 vertex_offset = 0;
    u32 vertex_count = 0;
    u32 index_offset = 0;
    u32 index_count = 0;
    GPUMaterialID material_id = GPUMaterialID::Invalid;
};

struct GPUMesh {
    std::vector<u32> primitive_indices = {};
};

struct GPUModel {
    std::vector<u32> mesh_indices = {};
};

struct GPUCameraData {
    glm::mat4 projection_mat = {};
    glm::mat4 view_mat = {};
    glm::mat4 projection_view_mat = {};
    glm::mat4 inv_view_mat = {};
    glm::mat4 inv_projection_view_mat = {};
    glm::vec3 position = {};
};

struct GPUModelTransformData {
    glm::mat4 model_transform_mat = {};
    glm::mat4 world_transform_mat = {};
};

struct GPUWorldData {
    SamplerID linear_sampler = SamplerID::Invalid;
    SamplerID nearest_sampler = SamplerID::Invalid;

    u64 cameras_ptr = {};
    u64 materials_ptr = {};
    u64 model_transforms_ptr = {};
    u32 active_camera_index = 0;

    GPUSunData sun = {};
    GPUAtmosphereData atmosphere = {};
};

// This struct should contain most generic and used object
// stuff like linear samplers, world buffers, etc...
struct WorldRenderContext {
    u64 world_ptr = 0;
    GPUWorldData world_data = {};
    ls::span<GPUModel> models = {};
    ls::span<GPUMesh> meshes = {};
    ls::span<GPUMeshPrimitive> mesh_primitives = {};
    BufferID vertex_buffer = BufferID::Invalid;
    BufferID index_buffer = BufferID::Invalid;
};

}  // namespace lr
