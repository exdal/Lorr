#include "Scene.hh"

#include "Components.hh"

namespace lr {
Scene::Scene(std::string_view name_, flecs::world &world_, flecs::entity &root_)
    : ecs(world_) {
    this->handle = ecs.entity(name_.data()).child_of(root_);
}

flecs::entity Scene::create_entity(this Scene &self, std::string_view name) {
    return self.ecs.entity(name.data()).child_of(self.handle);
}

void Scene::set_active_camera(this Scene &self, flecs::entity camera_entity) {
    ZoneScoped;

    self.ecs.each([](flecs::entity c, Component::ActiveCamera) { c.remove<Component::ActiveCamera>(); });
    camera_entity.add<Component::ActiveCamera>();
}

flecs::entity Scene::get_active_camera(this Scene &self) {
    ZoneScoped;

    flecs::entity e = flecs::entity::null();
    self.ecs.each([&](flecs::entity c, Component::ActiveCamera) { e = c; });

    return e;
}

flecs::entity Scene::create_perspective_camera(this Scene &self, std::string_view name, const glm::vec3 &position, f32 fov, f32 aspect) {
    ZoneScoped;

    return self
        .create_entity(name)  //
        .add<Component::PerspectiveCamera>()
        .set<Component::Transform>({ .position = position })
        .set<Component::Camera>({ .fov = fov, .aspect_ratio = aspect });
}

flecs::entity Scene::create_directional_light(this Scene &self, std::string_view name, const glm::vec2 &direction) {
    ZoneScoped;

    auto rotation_3d = glm::vec3(direction, 0.0);
    return self
        .create_entity(name)  //
        .set<Component::Transform>({ .rotation = rotation_3d })
        .set<Component::DirectionalLight>({});
}

}  // namespace lr
