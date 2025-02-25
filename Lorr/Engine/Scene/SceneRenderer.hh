#pragma once

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct SceneComposeInfo {
    ls::span<GPU::Model> gpu_models = {};
    ls::span<GPU::MeshletInstance> gpu_meshlet_instances = {};
};

struct SceneRenderInfo {
    vuk::Format format = vuk::Format::eR8G8B8A8Srgb;
    vuk::Extent3D extent = {};

    GPU::Scene scene_info = {};
    ls::span<GPU::TransformID> dirty_transform_ids = {};
    ls::span<GPU::Transforms> transforms = {};

    // Pass flags
    bool render_atmos : 1 = true;
    bool render_clouds : 1 = true;
};

struct SceneRenderer {
    auto init(this SceneRenderer &, Device *device) -> bool;
    auto destroy(this SceneRenderer &) -> void;

    auto setup_persistent_resources(this SceneRenderer &) -> void;

    // Scene
    auto compose(this SceneRenderer &, SceneComposeInfo &compose_info) -> void;
    auto render(this SceneRenderer &, SceneRenderInfo &render_info) -> vuk::Value<vuk::ImageAttachment>;

private:
    Device *device = nullptr;

    vuk::PersistentDescriptorSet bindless_set = {};
    Buffer transforms_buffer = {};

    u32 meshlet_instance_count = 0;
    Buffer models_buffer = {};
    Buffer meshlet_instance_buffer = {};

    Pipeline grid_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Pipeline sky_transmittance_pipeline = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Pipeline sky_multiscatter_pipeline = {};
    Pipeline sky_view_pipeline = {};
    vuk::Extent3D sky_view_lut_extent = { .width = 208, .height = 128, .depth = 1 };
    Pipeline sky_aerial_perspective_pipeline = {};
    vuk::Extent3D sky_aerial_perspective_lut_extent = { .width = 32, .height = 32, .depth = 32 };
    Pipeline sky_final_pipeline = {};

    Pipeline cloud_noise_pipeline = {};
    Image cloud_shape_noise_lut = {};
    ImageView cloud_shape_noise_lut_view = {};
    Image cloud_detail_noise_lut = {};
    ImageView cloud_detail_noise_lut_view = {};
    Pipeline cloud_apply_pipeline = {};
    vuk::Extent3D cloud_noise_lut_extent = { .width = 208, .height = 128, .depth = 128 };

    Pipeline vis_cull_meshlets_pipeline = {};
    Pipeline vis_cull_triangles_pipeline = {};
    Pipeline vis_triangle_id_pipeline = {};

    Pipeline tonemap_pipeline = {};
};

}  // namespace lr
