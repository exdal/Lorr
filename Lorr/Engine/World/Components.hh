#pragma once

#include "Model.hh"

#include <flecs.h>
#include <glm/gtx/quaternion.hpp>

namespace lr::Component {
struct Transform {
    constexpr static auto ICON = Icon::fa::up_down_left_right;

    glm::vec3 position = { 0.0, 0.0, 0.0 };
    glm::vec3 scale = { 1.0, 1.0, 1.0 };
    glm::vec3 rotation = {};
    glm::mat4 matrix = {};

    static void reflect(flecs::world &w) {
        w.component<Transform>("lr.Component.Transform")  //
            .member<glm::vec3, Transform>("position", &Transform::position)
            .member<glm::vec3, Transform>("scale", &Transform::scale)
            .member<glm::vec3, Transform>("rotation", &Transform::rotation);
    }
};

struct Camera {
    constexpr static auto ICON = Icon::fa::camera;

    glm::quat orientation = {};
    glm::mat4 projection = {};
    glm::vec3 velocity = {};
    f32 fov = 60.0f;
    f32 aspect_ratio = 1.777f;

    static void reflect(flecs::world &w) {
        w.component<Camera>("lr.Component.Camera")  //
            .member<glm::vec3, Camera>("velocity", &Camera::velocity)
            .member<f32, Camera>("fov", &Camera::fov)
            .member<f32, Camera>("aspect_ratio", &Camera::aspect_ratio);
    }
};

struct RenderableModel {
    constexpr static auto ICON = Icon::fa::cube;

    using ModelID_t = std::underlying_type_t<ModelID>;
    ModelID_t model_id = ~0_u32;

    static void reflect(flecs::world &w) {
        w.component<RenderableModel>("lr.Component.RenderableModel")  //
            .member<ModelID_t, RenderableModel>("model_id", &RenderableModel::model_id);
    }
};

struct DirectionalLight {
    constexpr static auto ICON = Icon::fa::sun;

    f32 intensity = 10.0f;
    static void reflect(flecs::world &w) {
        w.component<DirectionalLight>("lr.Component.DirectionalLight")  //
            .member<f32, DirectionalLight>("intensity", &DirectionalLight::intensity);
    }
};

/// TAGS ///

struct ActiveCamera {
    static void reflect(flecs::world &w) { w.component<ActiveCamera>("lr.Component.ActiveCamera"); }
};

struct EditorSelected {
    static void reflect(flecs::world &w) { w.component<EditorSelected>("lr.Component.EditorSelected"); }
};

struct PerspectiveCamera {
    static void reflect(flecs::world &w) { w.component<PerspectiveCamera>("lr.Component.PerspectiveCamera"); }
};

struct OrthographicCamera {
    static void reflect(flecs::world &w) { w.component<OrthographicCamera>("lr.Component.OrthographicCamera"); }
};

constexpr static std::tuple<  //
    Transform,
    Camera,
    RenderableModel,
    DirectionalLight,
    ActiveCamera,
    EditorSelected,
    PerspectiveCamera,
    OrthographicCamera>
    ALL_COMPONENTS;

template<typename T>
constexpr static void do_reflect(flecs::world &w) {
    T::reflect(w);
}

constexpr static void reflect_all(flecs::world &w) {
    std::apply(
        [&w](const auto &...args) {  //
            (do_reflect<std::decay_t<decltype(args)>>(w), ...);
        },
        ALL_COMPONENTS);
}

}  // namespace lr::Component
