#include "World.hh"

#include "Engine/Core/Application.hh"
#include "Engine/OS/File.hh"
#include "Engine/World/Components.hh"

#include <simdjson.h>

namespace lr {
template<typename T>
concept HasIcon = requires { T::ICON; };

template<typename T>
concept NotHasIcon = !requires { T::ICON; };

template<HasIcon T>
constexpr static auto do_get_icon() {
    return T::ICON;
}

template<NotHasIcon T>
constexpr static Icon::detail::icon_t do_get_icon() {
    return nullptr;
}

auto World::init(this World &self) -> bool {
    ZoneScoped;

    auto impl = new Impl;
    auto self = World(impl);

    Component::reflect_all(impl->ecs);

    //  ── SETUP DEFAULT SYSTEMS ───────────────────────────────────────────
    impl->ecs.system<Component::PerspectiveCamera, Component::Transform, Component::Camera>()
        .kind(flecs::OnUpdate)
        .each([](flecs::iter &it, usize, Component::PerspectiveCamera, Component::Transform &t, Component::Camera &c) {
            t.rotation.x = std::fmod(t.rotation.x, 360.0f);
            t.rotation.y = glm::clamp(t.rotation.y, -89.0f, 89.0f);

            auto inv_orient = glm::conjugate(c.orientation);
            t.position += glm::vec3(inv_orient * glm::vec4(c.cur_axis_velocity * it.delta_time(), 0.0f));

            c.projection = glm::perspectiveLH(glm::radians(c.fov), c.aspect_ratio, c.near_clip, c.far_clip);
            c.projection[1][1] *= -1;

            auto rotation = glm::radians(t.rotation);
            c.orientation = glm::angleAxis(rotation.x, glm::vec3(0.0f, -1.0f, 0.0f));
            c.orientation = glm::angleAxis(rotation.y, glm::vec3(1.0f, 0.0f, 0.0f)) * c.orientation;
            c.orientation = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)) * c.orientation;
            c.orientation = glm::normalize(c.orientation);

            glm::mat4 rotation_mat = glm::toMat4(c.orientation);
            glm::mat4 translation_mat = glm::translate(glm::mat4(1.f), -t.position);
            t.matrix = rotation_mat * translation_mat;
        });

    impl->ecs
        .system<Component::Transform>()  //
        .kind(flecs::OnUpdate)
        .without<Component::Camera>()
        .each([](flecs::iter &, usize, Component::Transform &t) {
            auto rotation = glm::radians(t.rotation);

            t.matrix = glm::translate(glm::mat4(1.0), t.position);
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
            t.matrix *= glm::scale(glm::mat4(1.0), t.scale);
        });

    // initialize component icons
    auto get_icon_of = [&](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        flecs::entity e = impl->ecs.entity<T>();
        impl->component_icons.emplace(e.raw_id(), do_get_icon<T>());
    };

    std::apply(
        [&](const auto &...args) {  //
            (get_icon_of(std::decay_t<decltype(args)>()), ...);
        },
        Component::ALL_COMPONENTS);

    return self;
}

auto World::create_from_file(const fs::path &path) -> ls::option<World> {
    ZoneScoped;

    ZoneScoped;
    namespace sj = simdjson;

    const fs::path &project_root = path.parent_path();
    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to read '{}'!", path);
        return ls::nullopt;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse world file! {}", sj::error_message(doc.error()));
        return ls::nullopt;
    }

    auto name_json = doc["name"];
    if (name_json.error() || !name_json.is_string()) {
        LOG_ERROR("World file must have `name` value as string.");
        return ls::nullopt;
    }

    auto world_name = std::string(name_json.get_string().value());
    return World::create(world_name);
}

auto World::destroy() -> void {
    ZoneScoped;

    impl->ecs.quit();

    delete impl;
    impl = nullptr;
}

auto World::set_active_scene(SceneID scene_id) -> void {
    ZoneScoped;

    impl->active_scene = scene_id;
}

auto World::active_scene() -> ls::option<SceneID> {
    ZoneScoped;

    return impl->active_scene;
}

auto World::component_icon(flecs::id_t id) -> Icon::detail::icon_t {
    return impl->component_icons[id];
}

}  // namespace lr
