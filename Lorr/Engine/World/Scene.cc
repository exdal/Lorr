#include "Engine/World/Scene.hh"

#include "Engine/World/Components.hh"
#include "Engine/World/World.hh"

namespace lr {
Scene::Scene(const std::string &name_, World &world_)
    : world(world_),
      handle(this->world.create_entity(name_)),
      editor_camera(nullptr) {
    ZoneScoped;

    this->editor_camera = this->create_perspective_camera("editor_camera", { 0.0, 2.0, 0.0 }, { 0, 0, 0 }, 65.0, 1.6);
    this->editor_camera  //
        ->add<Component::Hidden>()
        .add<Component::ActiveCamera>();
}

auto Scene::create_entity(const std::string &name) -> flecs::entity {
    ZoneScoped;

    return this->world.create_entity(name).child_of(this->handle);
}

auto Scene::create_perspective_camera(
    const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio) -> flecs::entity * {
    ZoneScoped;

    auto camera_entity_result = this->cameras.create();
    if (!camera_entity_result.has_value()) {
        return {};
    }

    auto &camera_entity = camera_entity_result->self;
    *camera_entity = this->create_entity(name)  //
                         .add<Component::PerspectiveCamera>()
                         .set<Component::Transform>({ .position = position, .rotation = rotation })
                         .set<Component::Camera>({ .fov = fov, .aspect_ratio = aspect_ratio, .index = camera_entity_result->id });

    return camera_entity;
}
}  // namespace lr
