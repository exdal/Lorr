#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
// This struct should contain most generic and used object
// stuff like linear samplers, world buffers, etc...
struct WorldRenderContext {
    SamplerID linear_sampler = SamplerID::Invalid;
    SamplerID nearest_sampler = SamplerID::Invalid;
    BufferID world_buffer = BufferID::Invalid;
};

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

struct GPUCameraData {
    glm::mat4 projection_mat = {};
    glm::mat4 view_mat = {};
    glm::mat4 projection_view_mat = {};
    glm::mat4 inv_view_mat = {};
    glm::mat4 inv_projection_view_mat = {};
    glm::vec3 position = {};
};

struct GPUWorldData {
    u64 cameras = {};
    u64 materials = {};
    u64 models = {};
    GPUSunData sun = {};
    GPUAtmosphereData atmosphere = {};
};

}  // namespace lr
