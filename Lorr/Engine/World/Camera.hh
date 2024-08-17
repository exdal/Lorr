#pragma once

#include <glm/gtx/quaternion.hpp>

namespace lr {
struct PerspectiveCamera {
    PerspectiveCamera() = default;
    PerspectiveCamera(const glm::vec3 &position_, f32 fov_, f32 aspect_)
        : position(position_),
          fov(fov_),
          aspect_ratio(aspect_) {}

    glm::vec3 position = {};
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;

    glm::vec3 velocity = {};
    f32 fov = 60.f;
    f32 aspect_ratio = 1.7777;

    glm::quat orientation = {};
    glm::mat4 projection_matrix = {};
    glm::mat4 view_matrix = {};
    glm::mat4 proj_view_matrix = {};

    void calc_proj_matrix(this PerspectiveCamera &);
    void calc_view_matrix(this PerspectiveCamera &);

    void update(this PerspectiveCamera &, f32 delta_time);
};

}  // namespace lr
