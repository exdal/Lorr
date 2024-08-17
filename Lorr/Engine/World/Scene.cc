#include <utility>

#include "Components.hh"
#include "Scene.hh"

namespace lr {
Scene::Scene(std::string name_, flecs::world &world_)
    : name(std::move(name_)),
      ecs(world_) {
}

flecs::entity Scene::create_entity(this Scene &self, std::string_view name) {
    return self.ecs.entity(name.data());
}

void Scene::set_active_camera(this Scene &self, flecs::entity camera_entity) {
    ZoneScoped;

    self.active_camera = camera_entity;
}

flecs::entity Scene::create_perspective_camera(this Scene &self, const glm::vec3 &position, f32 fov, f32 aspect) {
    u32 camera_id = self.cameras.size();
    self.cameras.emplace_back(position, fov, aspect);
    auto camera_entity = self.create_entity("camera").set<CameraComp>({ camera_id });

    return camera_entity;
}

}  // namespace lr
