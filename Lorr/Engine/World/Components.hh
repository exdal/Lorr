#pragma once

#include <flecs.h>
#include <glm/gtx/quaternion.hpp>

namespace lr::Component {
// NOTE: Components without `reflect` functions will not be shown in inspector panel

struct Icon {
    const c8 *code = nullptr;
};

struct Transform {
    glm::vec3 position = {};
    glm::vec3 scale = {};
    glm::vec3 rotation = {};
    glm::mat4 matrix = {};

    static void reflect(flecs::world &w) {
        w.component<Transform>()  //
            .member<glm::vec3, Transform>("position", &Transform::position)
            .member<glm::vec3, Transform>("scale", &Transform::scale)
            .member<glm::vec3, Transform>("rotation", &Transform::rotation);
    }
};

struct Camera {
    glm::quat orientation = {};
    glm::mat4 projection = {};
    glm::vec3 velocity = {};
    f32 fov = 60.0f;
    f32 aspect_ratio = 1.777f;

    static void reflect(flecs::world &w) {
        w.component<Camera>()  //
            .member<glm::vec3, Camera>("velocity", &Camera::velocity)
            .member<f32, Camera>("fov", &Camera::fov)
            .member<f32, Camera>("aspect_ratio", &Camera::aspect_ratio);
    }
};

struct EditorSelected {};
struct ActiveCamera {};
}  // namespace lr::Component

namespace lr::Prefab {
struct PerspectiveCamera {};
struct OrthographicCamera {};
}  // namespace lr::Prefab
