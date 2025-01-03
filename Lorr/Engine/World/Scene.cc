#include "Engine/World/Scene.hh"

#include "Engine/OS/File.hh"

#include "Engine/Util/JsonWriter.hh"

#include "Engine/World/Components.hh"
#include "Engine/World/World.hh"

#include <simdjson.h>

template<glm::length_t N, typename T>
bool json_to_vec(simdjson::ondemand::value &o, glm::vec<N, T> &vec) {
    using U = glm::vec<N, T>;
    for (i32 i = 0; i < U::length(); i++) {
        constexpr static std::string_view components[] = { "x", "y", "z", "w" };
        vec[i] = static_cast<T>(o[components[i]].get_double());
    }

    return true;
}

namespace lr {
template<>
struct Handle<Scene>::Impl {
    World *world = nullptr;
    flecs::entity handle = {};

    std::vector<flecs::entity> cameras = {};
};

auto Scene::create(const std::string &name, World *world) -> ls::option<Scene> {
    auto impl = new Impl;
    auto self = Scene(impl);

    impl->world = world;
    impl->handle = world->create_entity(name);

    world->ecs()
        .observer<Component::Camera>()  //
        .event(flecs::Monitor)
        .each([impl](flecs::iter &it, usize i, Component::Camera &camera) {
            if (it.event() == flecs::OnAdd) {
                camera.index = impl->cameras.size();
                impl->cameras.push_back(it.entity(i));
            } else {
                // TODO: Clear vector and reinsert all entities
            }
        });

    return self;
}

auto Scene::create_from_file(const fs::path &path, World *world) -> ls::option<Scene> {
    ZoneScoped;
    memory::ScopedStack stack;
    namespace sj = simdjson;

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return ls::nullopt;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse scene file! {}", sj::error_message(doc.error()));
        return ls::nullopt;
    }

    auto name_json = doc["name"].get_string();
    if (name_json.error()) {
        LOG_ERROR("Scene files must have names!");
        return ls::nullopt;
    }

    auto self = Scene::create(std::string(name_json.value()), world);

    auto entities_json = doc["entities"].get_array();
    for (auto entity_json : entities_json) {
        auto entity_name_json = entity_json["name"];
        if (entity_name_json.error()) {
            LOG_ERROR("Entities must have names!");
            return ls::nullopt;
        }

        auto entity_name = entity_name_json.get_string().value();
        auto e = self->create_entity(std::string(entity_name.begin(), entity_name.end()));

        auto entity_tags_json = entity_json["tags"];
        for (auto entity_tag : entity_tags_json.get_array()) {
            auto tag = world->ecs().component(stack.null_terminate(entity_tag.get_string()).data());
            e.add(tag);
        }

        auto components_json = entity_json["components"];
        for (auto component_json : components_json.get_array()) {
            auto component_name_json = component_json["name"];
            if (component_name_json.error()) {
                LOG_ERROR("Entity '{}' has corrupt components JSON array.", e.name());
                return ls::nullopt;
            }

            auto component_name = stack.null_terminate(component_name_json.get_string());
            auto component_id = world->ecs().lookup(component_name.data());
            if (!component_id) {
                LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name(), component_name);
                return ls::nullopt;
            }

            e.add(component_id);
            Component::Wrapper component(e, component_id);
            component.for_each([&](usize &, std::string_view member_name, Component::Wrapper::Member &member) {
                auto member_json = component_json[member_name];
                std::visit(
                    match{
                        [](const auto &) {},
                        [&](f32 *v) { *v = static_cast<f32>(member_json.get_double()); },
                        [&](i32 *v) { *v = static_cast<i32>(member_json.get_int64()); },
                        [&](u32 *v) { *v = member_json.get_uint64(); },
                        [&](i64 *v) { *v = member_json.get_int64(); },
                        [&](u64 *v) { *v = member_json.get_uint64(); },
                        [&](glm::vec2 *v) { json_to_vec(member_json.value(), *v); },
                        [&](glm::vec3 *v) { json_to_vec(member_json.value(), *v); },
                        [&](glm::vec4 *v) { json_to_vec(member_json.value(), *v); },
                        [&](std::string *v) { *v = member_json.get_string().value(); },
                        [&](UUID *v) { *v = UUID::from_string(member_json.get_string().value()).value(); },
                    },
                    member);
            });
        }
    }

    return self;
}

auto Scene::destroy() -> void {
    ZoneScoped;

    delete impl;
    impl = nullptr;
}

auto Scene::export_to_file(const fs::path &path) -> bool {
    ZoneScoped;

    JsonWriter json;
    json.begin_obj();
    json["name"] = impl->handle.name();
    json["entities"].begin_array();
    impl->handle.children([&](flecs::entity e) {
        json.begin_obj();
        json["name"] = std::string_view(e.name(), e.name().length());

        std::vector<Component::Wrapper> components = {};

        json["tags"].begin_array();
        e.each([&](flecs::id component_id) {
            auto world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            Component::Wrapper component(e, component_id);
            if (!component.has_component()) {
                json << component.path;
            } else {
                components.emplace_back(e, component_id);
            }
        });
        json.end_array();

        json["components"].begin_array();
        for (auto &component : components) {
            json.begin_obj();
            json["name"] = component.path;
            component.for_each([&](usize &, std::string_view member_name, Component::Wrapper::Member &member) {
                auto &member_json = json[member_name];
                std::visit(
                    match{
                        [](const auto &) {},
                        [&](f32 *v) { member_json = *v; },
                        [&](i32 *v) { member_json = *v; },
                        [&](u32 *v) { member_json = *v; },
                        [&](i64 *v) { member_json = *v; },
                        [&](u64 *v) { member_json = *v; },
                        [&](glm::vec2 *v) { member_json = *v; },
                        [&](glm::vec3 *v) { member_json = *v; },
                        [&](glm::vec4 *v) { member_json = *v; },
                        [&](std::string *v) { member_json = *v; },
                        [&](UUID *v) { member_json = v->str().c_str(); },
                    },
                    member);
            });
            json.end_obj();
        }
        json.end_array();
        json.end_obj();
    });

    json.end_array();
    json.end_obj();

    File file(path, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    file.write(json.stream.view().data(), json.stream.view().length());

    return true;
}

auto Scene::create_entity(const std::string &name) -> flecs::entity {
    ZoneScoped;

    return impl->world->create_entity(name).child_of(impl->handle);
}

auto Scene::create_perspective_camera(
    const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio) -> flecs::entity {
    ZoneScoped;

    u32 camera_id = impl->cameras.size();
    auto e = this->create_entity(name)  //
                 .add<Component::PerspectiveCamera>()
                 .set<Component::Transform>({ .position = position, .rotation = rotation })
                 .set<Component::Camera>({ .fov = fov, .aspect_ratio = aspect_ratio, .index = camera_id });
    impl->cameras.push_back(e);

    return e;
}

auto Scene::create_editor_camera() -> void {
    ZoneScoped;

    this->create_perspective_camera("editor_camera", { 0.0, 2.0, 0.0 }, { 0, 0, 0 }, 65.0, 1.6)
        .add<Component::Hidden>()
        .add<Component::EditorCamera>()
        .add<Component::ActiveCamera>();
}

auto Scene::root() -> flecs::entity {
    ZoneScoped;

    return impl->handle;
}

auto Scene::cameras() -> ls::span<flecs::entity> {
    ZoneScoped;

    return impl->cameras;
}

auto Scene::editor_camera() -> flecs::entity {
    ZoneScoped;

    for (const auto &v : impl->cameras) {
        if (v.has<Component::EditorCamera>()) {
            return v;
        }
    }

    return {};
}

auto Scene::name() -> std::string {
    ZoneScoped;

    return { impl->handle.name() };
}

auto Scene::name_sv() -> std::string_view {
    return { impl->handle.name().c_str(), impl->handle.name().length() };
}

}  // namespace lr
