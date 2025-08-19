#pragma once

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct EditorCamera : GPU::Camera {
    glm::vec3 rotation = {};
    glm::vec3 velocity = {};
    f32 fov = 65.0f;
    f32 max_velocity = 2.0f;
    f32 accel_speed = 8.0f;
    f32 decel_speed = 12.0f;

    auto update(this EditorCamera &, f32 delta_time, const glm::vec3 &target_velocity) -> void;

    auto aspect_ratio() -> f32 {
        return resolution.x / resolution.y;
    }
};

} // namespace lr
