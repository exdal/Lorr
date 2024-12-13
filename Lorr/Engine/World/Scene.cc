#include "Engine/World/Scene.hh"

#include "Engine/World/Components.hh"
#include "Engine/World/World.hh"

namespace lr {
template<>
struct Handle<Scene>::Impl {
    World *world = nullptr;
    flecs::entity handle = {};

    u32 editor_camera_index = {};
    std::vector<flecs::entity> cameras = {};
};

auto Scene::create(const std::string &name, World *world) -> ls::option<Scene> {
    auto impl = new Impl;
    auto self = Scene(impl);

    impl->world = world;
    impl->handle = world->create_entity(name);

    impl->editor_camera_index = self.create_perspective_camera("editor_camera", { 0.0, 2.0, 0.0 }, { 0, 0, 0 }, 65.0, 1.6);
    impl->cameras[impl->editor_camera_index].add<Component::Hidden>().add<Component::ActiveCamera>();

    return self;
}

auto Scene::destroy() -> void {
    ZoneScoped;

    delete impl;
    impl = nullptr;
}

auto Scene::create_entity(const std::string &name) -> flecs::entity {
    ZoneScoped;

    return impl->world->create_entity(name).child_of(impl->handle);
}

auto Scene::create_perspective_camera(
    const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio) -> u32 {
    ZoneScoped;

    u32 camera_id = impl->cameras.size();
    auto e = this->create_entity(name)  //
                 .add<Component::PerspectiveCamera>()
                 .set<Component::Transform>({ .position = position, .rotation = rotation })
                 .set<Component::Camera>({ .fov = fov, .aspect_ratio = aspect_ratio, .index = camera_id });
    impl->cameras.push_back(e);

    return camera_id;
}

auto Scene::root() -> flecs::entity {
    ZoneScoped;

    return impl->handle;
}

auto Scene::cameras() -> ls::span<flecs::entity> {
    ZoneScoped;

    return impl->cameras;
}

auto Scene::editor_camera_index() -> u32 {
    ZoneScoped;

    return impl->editor_camera_index;
}

auto Scene::name() -> std::string {
    ZoneScoped;

    return { impl->handle.name() };
}

auto Scene::name_sv() -> std::string_view {
    return { impl->handle.name().c_str(), impl->handle.name().length() };
}

}  // namespace lr
