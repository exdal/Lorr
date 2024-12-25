#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/World/RenderContext.hh"

namespace lr {
struct WorldRenderer : Handle<WorldRenderer> {
    struct SceneBeginInfo {
        u32 active_camera = 0;
        ls::span<GPUCameraData> gpu_cameras = {};
        ls::span<GPUModelTransformData> gpu_model_transforms = {};
        BufferID materials_buffer_id = BufferID::Invalid;
        ls::span<GPUModel> gpu_models = {};
    };

    static auto create(Device *device) -> WorldRenderer;

    auto setup_persistent_resources() -> void;

    auto set_scene_data(SceneBeginInfo &info) -> void;

    auto update_world_data() -> void;
    auto render(vuk::Value<vuk::ImageAttachment> &&target_attachment) -> vuk::Value<vuk::ImageAttachment>;
    auto draw_profiler_ui() -> void;

    auto sun_angle() -> glm::vec2 &;
    auto update_sun_dir() -> void;
    auto world_data() -> GPUWorldData &;
    auto composition_image() -> Image;
};

}  // namespace lr
