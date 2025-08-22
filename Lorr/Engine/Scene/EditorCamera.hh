#pragma once

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct EditorCamera : GPU::Camera {
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;
    f32 fov = 65.0f;
    glm::vec3 velocity = {};
    f32 max_velocity = 2.0f;
    f32 accel_speed = 8.0f;
    f32 decel_speed = 12.0f;

    auto update(this EditorCamera &, f32 delta_time, const glm::vec3 &target_velocity) -> void;

    auto direction(this EditorCamera &) -> glm::vec3;
    auto aspect_ratio() -> f32 {
        return resolution.x / resolution.y;
    }
};

} // namespace lr
