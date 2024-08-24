#include "Scene.hh"

#include "Camera.hh"

#include <simdjson.h>

namespace lr {
Scene::Scene(std::string name_, flecs::world &world_)
    : name(std::move(name_)),
      ecs(world_) {
    this->handle = ecs.entity();
}

Scene::Scene(const fs::path &path_, flecs::world &world_)
    : ecs(world_) {
}

void Scene::export_to(this Scene &self, const fs::path &path) {
    ZoneScoped;
}

void Scene::setup_systems(this Scene &self) {
    ZoneScoped;

    self.ecs.system<PerspectiveCamera>().each([](flecs::iter &it, usize, PerspectiveCamera &cam) {  //
        cam.update(it.delta_time());
    });
}

flecs::entity Scene::create_entity(this Scene &self, std::string_view name) {
    return self.ecs.entity(name.data()).child_of(self.handle);
}

void Scene::set_active_camera(this Scene &self, flecs::entity camera_entity) {
    ZoneScoped;

    self.active_camera = camera_entity;
}

flecs::entity Scene::create_perspective_camera(this Scene &self, const glm::vec3 &position, f32 fov, f32 aspect) {
    auto camera_entity = self.create_entity("camera").set<PerspectiveCamera>({ position, fov, aspect });
    return camera_entity;
}

}  // namespace lr
