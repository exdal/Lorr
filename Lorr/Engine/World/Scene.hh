#pragma once

#include "Engine/Core/Handle.hh"

#include "Engine/Memory/PagedPool.hh"

#include <flecs.h>

namespace lr {
using World_H = Handle<struct World>;

enum class SceneID : u32 { Invalid = ~0_u32 };
struct Scene {
    World &world;
    flecs::entity handle;

    // Camera handles need to be pointers, look below
    flecs::entity *editor_camera;
    PagedPool<flecs::entity, u32, 64> cameras = {};

    Scene(const std::string &name_, World &world_);
    virtual ~Scene() = default;

    auto create_entity(const std::string &name) -> flecs::entity;
    auto create_perspective_camera(const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
        -> flecs::entity *;

    template<typename T>
    void each_children(const T &f) {
        this->handle.children(f);
    }

    virtual auto load() -> bool = 0;
    virtual auto unload() -> bool = 0;
    virtual auto update() -> void = 0;
};

}  // namespace lr
