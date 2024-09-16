#pragma once

#include <flecs.h>

namespace lr {
enum class SceneID : u32 { Invalid = ~0_u32 };
struct Scene {
    flecs::world &ecs;
    flecs::entity handle;

    const std::string_view name() { return { handle.name(), handle.name().length() }; }

    Scene(std::string_view name_, flecs::world &world_);
    virtual ~Scene() = default;

    flecs::entity create_entity(this Scene &, std::string_view name);

    void set_active_camera(this Scene &, flecs::entity camera_entity);
    flecs::entity get_active_camera(this Scene &);

    flecs::entity create_perspective_camera(this Scene &, std::string_view name, const glm::vec3 &position, f32 fov, f32 aspect);
    flecs::entity create_directional_light(this Scene &, std::string_view name, const glm::vec2 &direction);

    template<typename T>
    void children(const T &f) {
        this->handle.children(f);
    }

    virtual bool do_load() = 0;
    virtual bool do_unload() = 0;
    virtual void do_update() = 0;
};

}  // namespace lr
