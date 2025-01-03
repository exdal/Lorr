#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/World/RenderContext.hh"

namespace lr {
struct WorldRenderer : Handle<WorldRenderer> {
    struct SceneBeginInfo {
        i32 camera_count = 0;
        i32 model_transform_count = 0;
    };

    struct GPUSceneData {
        u32 active_camera_index = 0;
        BufferID materials_buffer_id = BufferID::Invalid;
        ls::span<GPUModel> models = {};

        GPUCameraData *cameras = nullptr;
        GPUModelTransformData *model_transforms = nullptr;

        // Internals
        ls::option<usize> offset_cameras;
        ls::option<usize> offset_model_transforms;
        ls::option<usize> offset_models;
        usize upload_size = 0;
        TransientBuffer world_data = {};
    };

    static auto create(Device *device) -> WorldRenderer;
    auto setup_persistent_resources() -> void;

    auto begin_scene(const SceneBeginInfo &info) -> GPUSceneData;
    auto end_scene(GPUSceneData &scene_gpu_data) -> void;

    auto render(vuk::Value<vuk::ImageAttachment> &render_target) -> vuk::Value<vuk::ImageAttachment>;

    auto draw_profiler_ui() -> void;

    auto sun_angle() -> glm::vec2 &;
    auto update_sun_dir() -> void;
    auto world_data() -> GPUWorldData &;
};

}  // namespace lr
