#include "Engine/World/Scene.hh"

#include "Engine/OS/File.hh"

#include "Engine/Util/JsonWriter.hh"

#include "Engine/World/Components.hh"

#include <simdjson.h>

namespace lr {
template<glm::length_t N, typename T>
bool json_to_vec(simdjson::ondemand::value &o, glm::vec<N, T> &vec) {
    using U = glm::vec<N, T>;
    for (i32 i = 0; i < U::length(); i++) {
        constexpr static std::string_view components[] = { "x", "y", "z", "w" };
        vec[i] = static_cast<T>(o[components[i]].get_double());
    }

    return true;
}

auto Scene::init(this Scene &self, const std::string &name) -> bool {
    ZoneScoped;

    self.name = name;
    self.world
        .observer<Component::Camera>()  //
        .event(flecs::Monitor)
        .each([&self](flecs::iter &it, usize i, Component::Camera &camera) {
            if (it.event() == flecs::OnAdd) {
                camera.index = self.cameras.size();
                self.cameras.push_back(it.entity(i));
            } else {
                // TODO: Clear vector and reinsert all entities
            }
        });

    return true;
}

auto Scene::init_from_file(this Scene &self, const fs::path &path) -> bool {
    ZoneScoped;
    memory::ScopedStack stack;
    namespace sj = simdjson;

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse scene file! {}", sj::error_message(doc.error()));
        return false;
    }

    auto name_json = doc["name"].get_string();
    if (name_json.error()) {
        LOG_ERROR("Scene files must have names!");
        return false;
    }

    self.init(std::string(name_json.value()));

    auto entities_json = doc["entities"].get_array();
    for (auto entity_json : entities_json) {
        auto entity_name_json = entity_json["name"];
        if (entity_name_json.error()) {
            LOG_ERROR("Entities must have names!");
            return false;
        }

        auto entity_name = entity_name_json.get_string().value();
        auto e = self.create_entity(std::string(entity_name.begin(), entity_name.end()));

        auto entity_tags_json = entity_json["tags"];
        for (auto entity_tag : entity_tags_json.get_array()) {
            auto tag = self.world.component(stack.null_terminate(entity_tag.get_string()).data());
            e.add(tag);
        }

        auto components_json = entity_json["components"];
        for (auto component_json : components_json.get_array()) {
            auto component_name_json = component_json["name"];
            if (component_name_json.error()) {
                LOG_ERROR("Entity '{}' has corrupt components JSON array.", e.name());
                return false;
            }

            auto component_name = stack.null_terminate(component_name_json.get_string());
            auto component_id = self.world.lookup(component_name.data());
            if (!component_id) {
                LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name(), component_name);
                return false;
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

    return true;
}

auto Scene::destroy(this Scene &self) -> void {
    ZoneScoped;

    self.world.release();
}

auto Scene::export_to_file(this Scene &self, const fs::path &path) -> bool {
    ZoneScoped;

    JsonWriter json;
    json.begin_obj();
    json["name"] = self.name;
    json["entities"].begin_array();
    self.world.each([&](flecs::entity e) {
        json.begin_obj();
        json["name"] = std::string_view(e.name(), e.name().length());

        std::vector<Component::Wrapper> components = {};

        json["tags"].begin_array();
        e.each([&](flecs::id component_id) {
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

auto Scene::create_entity(this Scene &self, const std::string &name) -> flecs::entity {
    ZoneScoped;

    return self.world.entity(name);
}

auto Scene::create_perspective_camera(
    this Scene &self, const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
    -> flecs::entity {
    ZoneScoped;

    u32 camera_id = self.cameras.size();
    auto e = self.create_entity(name)  //
                 .add<Component::PerspectiveCamera>()
                 .set<Component::Transform>({ .position = position, .rotation = rotation })
                 .set<Component::Camera>({ .fov = fov, .aspect_ratio = aspect_ratio, .index = camera_id });
    self.cameras.push_back(e);

    return e;
}

auto Scene::create_editor_camera(this Scene &self) -> void {
    ZoneScoped;

    self.create_perspective_camera("editor_camera", { 0.0, 2.0, 0.0 }, { 0, 0, 0 }, 65.0, 1.6)
        .add<Component::Hidden>()
        .add<Component::EditorCamera>()
        .add<Component::ActiveCamera>();
}

auto Scene::upload_scene(this Scene &self, WorldRenderer &renderer) -> void {
    ZoneScoped;

    auto camera_query = self.world  //
                            .query_builder<Component::Transform, Component::Camera>()
                            .build();
    auto model_transform_query = self.world  //
                                     .query_builder<Component::Transform, Component::RenderableModel>()
                                     .build();

    auto scene_data = renderer.begin_scene({
        .camera_count = camera_query.count(),
        .model_transform_count = model_transform_query.count(),
    });

    u32 active_camera_index = 0;
    camera_query.each([&](flecs::entity e, Component::Transform &t, Component::Camera &c) {
        auto &camera_data = scene_data.cameras[c.index];
        camera_data.projection_mat = c.projection;
        camera_data.view_mat = t.matrix;
        camera_data.projection_view_mat = glm::transpose(c.projection * t.matrix);
        camera_data.inv_view_mat = glm::inverse(glm::transpose(t.matrix));
        camera_data.inv_projection_view_mat = glm::inverse(glm::transpose(c.projection)) * camera_data.inv_view_mat;
        camera_data.position = t.position;
        camera_data.near_clip = c.near_clip;
        camera_data.far_clip = c.far_clip;

        if (e.has<Component::ActiveCamera>()) {
            active_camera_index = c.index;
        }
    });

    renderer.end_scene(scene_data);
}

auto Scene::tick(this Scene &self) -> bool {
    return self.world.progress();
}

auto Scene::root(this Scene &self) -> const flecs::world & {
    ZoneScoped;

    return self.world;
}

auto Scene::editor_camera(this Scene &self) -> flecs::entity {
    ZoneScoped;

    for (const auto &v : self.cameras) {
        if (v.has<Component::EditorCamera>()) {
            return v;
        }
    }

    return {};
}

auto Scene::get_cameras(this Scene &self) -> ls::span<flecs::entity> {
    ZoneScoped;

    return self.cameras;
}

auto Scene::get_name(this Scene &self) -> const std::string & {
    ZoneScoped;

    return self.name;
}

auto Scene::get_name_sv(this Scene &self) -> std::string_view {
    return self.name;
}

}  // namespace lr
