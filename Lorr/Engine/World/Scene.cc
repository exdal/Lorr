#include "Scene.hh"

#include "Components.hh"

namespace lr {
Scene::Scene(std::string_view name_, flecs::world &world_)
    : ecs(world_) {
    this->handle = ecs.entity(name_.data());
}

flecs::entity Scene::create_entity(this Scene &self, std::string_view name) {
    return self.ecs.entity(name.data()).child_of(self.handle);
}

void Scene::set_active_camera(this Scene &self, flecs::entity camera_entity) {
    ZoneScoped;

    self.active_camera = camera_entity;
}

flecs::entity Scene::create_perspective_camera(this Scene &self, std::string_view name, const glm::vec3 &position, f32 fov, f32 aspect) {
    auto camera_entity = self.create_entity(name).is_a<Prefab::PerspectiveCamera>();

    auto *transform = camera_entity.get_mut<Component::Transform>();
    auto *camera = camera_entity.get_mut<Component::Camera>();

    transform->position = position;
    camera->fov = fov;
    camera->aspect_ratio = aspect;

    return camera_entity;
}

}  // namespace lr
