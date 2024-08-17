#pragma once

#include "Camera.hh"

#include <flecs.h>

namespace lr {
enum class SceneID : u32 { Invalid = ~0_u32 };
struct Scene {
    std::string name;
    flecs::world &ecs;
    std::vector<PerspectiveCamera> cameras = {};
    ls::option<flecs::entity> active_camera = ls::nullopt;

    Scene(std::string name_, flecs::world &world_);
    virtual ~Scene() = default;

    flecs::entity create_entity(this Scene &, std::string_view name);

    void set_active_camera(this Scene &, flecs::entity camera_entity);
    flecs::entity create_perspective_camera(this Scene &, const glm::vec3 &position, f32 fov, f32 aspect);

    virtual bool do_load() = 0;
    virtual bool do_unload() = 0;
    virtual void do_update() = 0;
};

}  // namespace lr
