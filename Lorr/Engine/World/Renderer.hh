#pragma once

#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr {

struct WorldRenderer {
    Device device = {};
    TaskGraph pbr_graph = {};
    TransferManager transfer_man = {};

    ls::option<WorldRenderContext> context = ls::nullopt;
    std::vector<GPUMeshPrimitive> gpu_mesh_primitives = {};
    std::vector<GPUMesh> gpu_meshes = {};
    std::vector<GPUModel> gpu_models = {};

    // Static buffers
    BufferID material_buffer = BufferID::Invalid;
    BufferID static_vertex_buffer = BufferID::Invalid;
    BufferID static_index_buffer = BufferID::Invalid;

    SamplerID linear_sampler = SamplerID::Invalid;
    SamplerID nearest_sampler = SamplerID::Invalid;

    TaskImageID swap_chain_image = TaskImageID::Invalid;
    ImageID editor_view_image = ImageID::Invalid;
    ImageID final_image = ImageID::Invalid;
    ImageID imgui_font_image = ImageID::Invalid;

    // Vis
    ImageID geo_depth_image = ImageID::Invalid;

    // Sky
    ImageID sky_transmittance_image = ImageID::Invalid;
    ImageID sky_multiscatter_image = ImageID::Invalid;
    ImageID sky_aerial_perspective_image = ImageID::Invalid;
    ImageID sky_view_image = ImageID::Invalid;

    // Cloud
    ImageID cloud_shape_noise_image = ImageID::Invalid;
    ImageID cloud_detail_noise_image = ImageID::Invalid;

    WorldRenderer(Device device_);

    auto setup_persistent_resources(this WorldRenderer &) -> void;

    auto record_setup_graph(this WorldRenderer &) -> void;
    auto record_pbr_graph(this WorldRenderer &, SwapChain &swap_chain) -> void;

    auto construct_scene(this WorldRenderer &) -> void;
    auto render(this WorldRenderer &, SwapChain &swap_chain, ImageID image_id) -> bool;
};

}  // namespace lr
