#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/Graphics/Vulkan.hh"
#include "Engine/World/RenderContext.hh"

namespace lr {
struct WorldRenderer : Handle<WorldRenderer> {
    struct PBRFlags {
        bool render_sky = true;
        bool render_aerial_perspective = true;
        bool render_clouds = false;
        bool render_editor_grid = true;
        bool render_imgui = true;
    };

    struct SceneBeginInfo {
        GPUAllocation cameras_allocation = {};
        GPUAllocation model_transfors_allocation = {};
        BufferID materials_buffer_id = BufferID::Invalid;
        ls::span<GPUModel> gpu_models = {};
    };

    static auto create(Device device) -> WorldRenderer;

    auto setup_persistent_resources() -> void;
    auto record_setup_graph() -> void;
    auto record_pbr_graph(SwapChain &swap_chain) -> void;
    auto get_pbr_flags() -> PBRFlags &;
    auto update_pbr_graph() -> void;

    auto begin_scene_render_data(u32 camera_count, u32 model_transform_count) -> ls::option<SceneBeginInfo>;
    auto end_scene_render_data(SceneBeginInfo &info) -> void;

    auto update_world_data() -> void;
    auto render(SwapChain &swap_chain, Image &image) -> bool;
    auto draw_profiler_ui() -> void;

    auto sun_angle() -> glm::vec2 &;
    auto update_sun_dir() -> void;
    auto world_data() -> GPUWorldData &;
    auto composition_image() -> Image;
};

}  // namespace lr
