#include "Scene.hh"

#include "Components.hh"

namespace lr {
Scene::Scene(std::string name_, flecs::world &world_)
    : name(std::move(name_)),
      ecs(world_) {
    this->handle = ecs.entity();
}

Scene::Scene(const fs::path &path_, flecs::world &world_)
    : ecs(world_) {
    import_from(path_);
}

bool Scene::import_from(this Scene &self, const fs::path &path) {
    ZoneScoped;

    auto path_str = path.string();

    return true;
}

void Scene::export_to(this Scene &self, const fs::path &path) {
    ZoneScoped;
}

void Scene::setup_systems(this Scene &self) {
    ZoneScoped;
}

flecs::entity Scene::create_entity(this Scene &self, std::string_view name) {
    return self.ecs.entity(name.data()).child_of(self.handle);
}

void Scene::set_active_camera(this Scene &self, flecs::entity camera_entity) {
    ZoneScoped;

    self.active_camera = camera_entity;
}

flecs::entity Scene::create_perspective_camera(this Scene &self, const glm::vec3 &position, f32 fov, f32 aspect) {
    auto camera_entity = self.create_entity("camera").is_a<Prefab::PerspectiveCamera>();
    return camera_entity;
}

}  // namespace lr
