#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace lr::Math {
inline auto quat_dir(const glm::vec3 &rotation) -> glm::quat {
    ZoneScoped;

    glm::quat orientation = {};
    orientation = glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    orientation = glm::angleAxis(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)) * orientation;
    orientation = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)) * orientation;
    return glm::normalize(orientation);
}
} // namespace lr::Math
