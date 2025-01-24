#include "Engine/Scene/Scene.hh"

#include "Engine/Core/Application.hh"

#include "Engine/OS/File.hh"

#include "Engine/Util/JsonWriter.hh"

#include "Engine/Scene/ECSModule/ComponentWrapper.hh"
#include "Engine/Scene/ECSModule/Core.hh"

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

bool json_to_quat(simdjson::ondemand::value &o, glm::quat &quat) {
    for (i32 i = 0; i < glm::quat::length(); i++) {
        constexpr static std::string_view components[] = { "x", "y", "z", "w" };
        quat[i] = static_cast<f32>(o[components[i]].get_double());
    }

    return true;
}

auto Scene::init(this Scene &self, const std::string &name) -> bool {
    ZoneScoped;

    self.name = name;
    self.world.emplace();
    self.imported_modules.emplace_back(self.world->import <ECS::Core>());
    self.root = self.world->entity();

    self.world
        ->observer<ECS::Camera>()  //
        .event(flecs::Monitor)
        .each([&self](flecs::iter &it, usize i, ECS::Camera &camera) {
            if (it.event() == flecs::OnAdd) {
                auto entity = it.entity(i);
                camera.id = self.cameras.create_slot(flecs::entity(entity));
                if (entity.has<ECS::EditorCamera>()) {
                    self.editor_camera_id = camera.id;
                }
            } else if (it.event() == flecs::OnRemove) {
                self.cameras.destroy_slot(camera.id);
            }
        });

    return true;
}

auto Scene::destroy(this Scene &self) -> void {
    ZoneScoped;

    self.name.clear();
    self.root.clear();
    self.cameras.reset();
    self.world.reset();
}

auto Scene::import_from_file(this Scene &self, const fs::path &path) -> bool {
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

    self.set_name(std::string(name_json.value()));

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
            auto tag = self.world->component(stack.null_terminate(entity_tag.get_string()).data());
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
            auto component_id = self.world->lookup(component_name.data());
            if (!component_id) {
                LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name(), component_name);
                return false;
            }

            e.add(component_id);
            ECS::ComponentWrapper component(e, component_id);
            component.for_each([&](usize &, std::string_view member_name, ECS::ComponentWrapper::Member &member) {
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
                        [&](glm::quat *v) { json_to_quat(member_json.value(), *v); },
                        //[&](glm::mat4 *v) { json_to_mat(member_json.value(), *v); },
                        [&](std::string *v) { *v = member_json.get_string().value(); },
                        [&](UUID *v) { *v = UUID::from_string(member_json.get_string().value()).value(); },
                    },
                    member);
            });
        }
    }

    return true;
}

auto Scene::export_to_file(this Scene &self, const fs::path &path) -> bool {
    ZoneScoped;

    JsonWriter json;
    json.begin_obj();
    json["name"] = self.name;
    json["entities"].begin_array();
    self.root.children([&](flecs::entity e) {
        json.begin_obj();
        json["name"] = std::string_view(e.name(), e.name().length());

        std::vector<ECS::ComponentWrapper> components = {};

        json["tags"].begin_array();
        e.each([&](flecs::id component_id) {
            if (!component_id.is_entity()) {
                return;
            }

            ECS::ComponentWrapper component(e, component_id);
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
            component.for_each([&](usize &, std::string_view member_name, ECS::ComponentWrapper::Member &member) {
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
                        [&](glm::quat *v) { member_json = *v; },
                        [&](glm::mat4 *v) { member_json = ls::span(glm::value_ptr(*v), 16); },
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

    auto name_sv = flecs::string_view(name.c_str());
    if (name.empty()) {
        memory::ScopedStack stack;

        auto e = self.world->entity().child_of(self.root);
        e.set_name(stack.format("Entity {}", e.raw_id()).data());
        return e;
    }

    return self.world->entity(name_sv).child_of(self.root);
}

auto Scene::create_perspective_camera(
    this Scene &self, const std::string &name, const glm::vec3 &position, const glm::vec3 &rotation, f32 fov, f32 aspect_ratio)
    -> flecs::entity {
    ZoneScoped;

    return self
        .create_entity(name)  //
        .add<ECS::PerspectiveCamera>()
        .set<ECS::Transform>({ .position = position, .rotation = rotation })
        .set<ECS::Camera>({ .fov = fov, .aspect_ratio = aspect_ratio });
}

auto Scene::create_editor_camera(this Scene &self) -> void {
    ZoneScoped;

    self.create_perspective_camera("editor_camera", { 0.0, 2.0, 0.0 }, { 0, 0, 0 }, 65.0, 1.6)
        .add<ECS::Hidden>()
        .add<ECS::EditorCamera>()
        .add<ECS::ActiveCamera>();
}

auto Scene::upload_scene(this Scene &self, SceneRenderer &renderer) -> void {
    ZoneScoped;

    auto &app = Application::get();
    // clang-format off
    auto camera_query = self.get_world()
        .query_builder<ECS::Transform, ECS::Camera>()
        .build();
    auto model_transform_query = self.get_world()
        .query_builder<ECS::Transform, ECS::RenderingModel>()
        .build();
    auto directional_light_query = self.get_world()
        .query_builder<ECS::DirectionalLight>()
        .build();
    auto atmosphere_query = self.get_world()
        .query_builder<ECS::Atmosphere>()
        .build();
    // clang-format on

    // TODO: this is too much
    u32 mesh_transform_count = 0;
    model_transform_query.each([&](flecs::entity, ECS::Transform &, ECS::RenderingModel &rendering_model) {  //
        auto *model = app.asset_man.get_model(rendering_model.model);
        if (!model) {
            return;
        }

        mesh_transform_count += model->meshes.size();
    });

    auto scene_data = renderer.begin_scene({
        .camera_count = static_cast<u32>(camera_query.count()),
        .mesh_transform_count = mesh_transform_count,
        .has_sun = directional_light_query.count() > 0,
        .has_atmosphere = atmosphere_query.count() > 0,
    });

    scene_data.materials_buffer_id = app.asset_man.material_buffer();

    u32 active_camera_index = 0;
    camera_query.each([&](flecs::entity e, ECS::Transform &t, ECS::Camera &c) {
        auto index = SlotMap_decode_id(c.id).index;
        auto &camera_data = scene_data.cameras[index];
        camera_data.projection_mat = c.projection;
        camera_data.view_mat = t.matrix;
        camera_data.projection_view_mat = glm::transpose(c.projection * t.matrix);
        camera_data.inv_view_mat = glm::inverse(glm::transpose(t.matrix));
        camera_data.inv_projection_view_mat = glm::inverse(glm::transpose(c.projection)) * camera_data.inv_view_mat;
        camera_data.position = t.position;
        camera_data.near_clip = c.near_clip;
        camera_data.far_clip = c.far_clip;

        if (e.has<ECS::ActiveCamera>()) {
            active_camera_index = index;
        }
    });

    directional_light_query.each([&](flecs::entity, ECS::DirectionalLight &light) {
        auto rad = glm::radians(light.direction);

        auto &sun_data = scene_data.sun;
        sun_data->direction = {
            glm::cos(rad.x) * glm::sin(rad.y),
            glm::sin(rad.x) * glm::sin(rad.y),
            glm::cos(rad.y),
        };
        sun_data->intensity = light.intensity;
    });

    atmosphere_query.each([&](flecs::entity, ECS::Atmosphere &atmos) {
        auto &atmos_data = scene_data.atmosphere;
        *atmos_data = {};  // default constructor
        atmos_data->rayleigh_scatter = atmos.rayleigh_scattering * 1e-3f;
        atmos_data->rayleigh_density = atmos.rayleigh_density;
        atmos_data->mie_scatter = atmos.mie_scattering * 1e-3f;
        atmos_data->mie_density = atmos.mie_density;
        atmos_data->mie_extinction = atmos.mie_extinction * 1e-3f;
        atmos_data->ozone_absorption = atmos.ozone_absorption * 1e-3f;
        atmos_data->ozone_height = atmos.ozone_height;
        atmos_data->ozone_thickness = atmos.ozone_thickness;
    });

    u32 mesh_transform_index = 0;
    scene_data.models.clear();
    auto mesh_transforms_ptr = scene_data.mesh_transforms;
    model_transform_query.each([&](flecs::entity, ECS::Transform &parent_transform, ECS::RenderingModel &rendering_model) {
        auto *model = app.asset_man.get_model(rendering_model.model);
        if (!model) {
            return;
        }

        auto &gpu_model = scene_data.models.emplace_back();
        for (const auto &meshlet : model->meshlets) {
            gpu_model.meshlets.push_back({
                .vertex_offset = meshlet.vertex_offset,
                .index_count = meshlet.index_count,
                .index_offset = meshlet.index_offset,
                .triangle_count = meshlet.triangle_count,
                .triangle_offset = meshlet.triangle_offset,
            });
        }

        for (const auto &mesh : model->meshes) {
            gpu_model.meshes.push_back({
                .meshlet_indices = mesh.meshlet_indices,
            });
        }

        for (auto &node : model->nodes) {
            gpu_model.nodes.push_back({
                .mesh_indices = node.mesh_indices,
                .transform_index = mesh_transform_index,
            });

            mesh_transform_index++;
            mesh_transforms_ptr->transform_mat = parent_transform.matrix * node.transform;
            mesh_transforms_ptr++;
        }

        gpu_model.vertex_bufffer_id = model->vertex_buffer.id();
        gpu_model.index_buffer_id = model->index_buffer.id();
    });

    renderer.end_scene(scene_data);
}

auto Scene::tick(this Scene &self) -> bool {
    return self.world->progress();
}

auto Scene::set_name(this Scene &self, const std::string &name) -> void {
    ZoneScoped;

    self.name = name;
}

auto Scene::get_root(this Scene &self) -> flecs::entity {
    ZoneScoped;

    return self.root;
}

auto Scene::get_world(this Scene &self) -> flecs::world & {
    ZoneScoped;

    return self.world.value();
}

auto Scene::editor_camera(this Scene &self) -> flecs::entity {
    ZoneScoped;

    return *self.cameras.slot(self.editor_camera_id);
}

auto Scene::get_name(this Scene &self) -> const std::string & {
    ZoneScoped;

    return self.name;
}

auto Scene::get_name_sv(this Scene &self) -> std::string_view {
    return self.name;
}

auto Scene::get_imported_modules(this Scene &self) -> ls::span<flecs::entity> {
    return self.imported_modules;
}

}  // namespace lr
