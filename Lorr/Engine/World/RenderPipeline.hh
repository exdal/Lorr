#pragma once

#include "Engine/World/RenderPasses.hh"

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

struct RenderPipeline {
    Device device = {};
    // Main task graph for entire pipeline
    TaskGraph task_graph = {};

    // Buffers for static objects
    BufferID persistent_vertex_buffer = {};
    BufferID persistent_index_buffer = {};
    BufferID persistent_material_buffer = {};

    GPUWorldData world_data = {};

    // Pipeline Info
    SamplerID linear_sampler = {};
    SamplerID nearest_sampler = {};
    TaskImageID swap_chain_image = {};
    // Passes
    ImguiPass imgui_pass = {};

    bool init(this RenderPipeline &, Device device);
    void shutdown(this RenderPipeline &);

    // Presentation
    void update_world_data(this RenderPipeline &);
    bool prepare(this RenderPipeline &, usize frame_index);
    bool render_into(this RenderPipeline &, SwapChain &swap_chain, ls::span<ImageID> images, ls::span<ImageViewID> image_views);
};

}  // namespace lr
