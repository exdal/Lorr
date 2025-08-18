#pragma once

#include "Engine/Scene/GPUScene.hh"

namespace lr {
struct EditorCamera : GPU::Camera {
    glm::vec3 rotation = {};
    glm::vec3 velocity = {};
    f32 fov = 65.0f;

    auto aspect_ratio() -> f32 {
        return resolution.x / resolution.y;
    }
};

} // namespace lr
