#pragma once

#include "Engine/Graphics/Vulkan.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/World/Model.hh"

namespace lr {
struct GPUSunData {
    glm::vec3 direction = {};
    f32 intensity = 10.0f;
};

struct GPUAtmosphereData {
    glm::vec3 rayleigh_scatter = { 0.005802f, 0.013558f, 0.033100f };
    f32 rayleigh_density = 8.0;

    glm::vec3 mie_scatter = { 0.003996, 0.003996, 0.003996 };
    f32 mie_density = 1.2;
    f32 mie_extinction = 0.004440;
    f32 mie_asymmetry = 0.8;

    glm::vec3 ozone_absorption = { 0.000650, 0.001881, 0.000085 };
    f32 ozone_height = 25.0;
    f32 ozone_thickness = 15.0;

    glm::vec3 terrain_albedo = { 0.3, 0.3, 0.3 };
    f32 aerial_km_per_slice = 8.0;
    f32 planet_radius = 6360.0;
    f32 atmos_radius = 6460.0;
};

struct GPUClouds {
    glm::vec2 bounds = { 2.0, 5.0 };
    f32 global_scale = 0.15;
    f32 shape_noise_scale = 0.3;
    glm::vec3 shape_noise_weights = { 0.625f, 0.25f, 0.125f };
    f32 detail_noise_scale = 0.3;
    glm::vec3 detail_noise_weights = { 0.625f, 0.25f, 0.125f };
    f32 detail_noise_influence = 0.4;
    f32 coverage = 0.9;
    f32 general_density = 0.1;
    glm::vec3 phase_values = { 0.623, 0.335, 0.979 };  // forwards phase, backwards phase, mix factor
    f32 cloud_type = 0.0;
    f32 draw_distance = 1000.0;
    f32 darkness_threshold = 0.450;
    i32 sun_step_count = 5;
    i32 clouds_step_count = 30;
    f32 cloud_light_absorption = 0.443;
    f32 sun_light_absorption = 0.224;
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

struct GPUModelTransformData {
    glm::mat4 model_transform_mat = {};
    glm::mat4 world_transform_mat = {};
};

// NOTE: THIS IS GPU WORLD DATA, DO NOT CHANGE THIS WITHOUT
// SHADER MODIFICATION!!! PLACE CPU RELATED STUFF AT `WorldRenderContext`!
//
struct GPUWorldData {
    SamplerID linear_sampler = SamplerID::Invalid;
    SamplerID nearest_sampler = SamplerID::Invalid;

    u64 cameras_ptr = 0;
    u64 materials_ptr = 0;
    u64 model_transforms_ptr = 0;
    u32 active_camera_index = 0;

    GPUSunData sun = {};
    GPUAtmosphereData atmosphere = {};
    GPUClouds clouds = {};
};

struct FrameResources {
    Image final_image = {};
    ImageView final_view = {};

    FrameResources(Device &device, vuk::Value<vuk::ImageAttachment> &target_attachment) {
        // clang-format off
        final_image = Image::create(
            device,
            target_attachment->format,
            vuk::ImageUsageFlagBits::eColorAttachment | vuk::ImageUsageFlagBits::eSampled,
            vuk::ImageType::e2D,
            target_attachment->extent
        ).value();
        final_view = ImageView::create(
            device,
            final_image,
            vuk::ImageUsageFlagBits::eColorAttachment | vuk::ImageUsageFlagBits::eSampled,
            vuk::ImageViewType::e2D,
            { .aspectMask = vuk::ImageAspectFlagBits::eColor }
        ).value();
        // clang-format on
    }

    ~FrameResources() {}
};

// This struct should contain most generic and used object
// stuff like linear samplers, world buffers, etc...
struct WorldRenderContext {
    u64 world_ptr = 0;
    GPUWorldData world_data = {};
    ls::span<GPUModel> models = {};

    glm::vec2 sun_angle = { 0.0, 45.0 };

    Sampler linear_sampler = {};
    Sampler nearest_sampler = {};

    Image imgui_font_image = {};
    ImageView imgui_font_view = {};
    Pipeline imgui_pipeline = {};
    std::vector<vuk::Value<vuk::ImageAttachment>> imgui_rendering_attachments = {};
    std::vector<ImageViewID> imgui_rendering_view_ids = {};

    Pipeline grid_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Image sky_aerial_perspective_lut = {};
    ImageView sky_aerial_perspective_lut_view = {};
    Image sky_view_lut = {};
    ImageView sky_view_lut_view = {};
    Pipeline sky_view_pipeline = {};

    // FRAME IMAGES
    // Images that DO rely on Swap Chain dimensions
    std::unique_ptr<FrameResources> frame_resources = {};
};

}  // namespace lr
