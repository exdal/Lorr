#include "Engine/Scene/Scene.hh"

#include "Engine/Core/Application.hh"

#include "Engine/OS/File.hh"

#include "Engine/Util/JsonWriter.hh"

#include "Engine/Scene/ECSModule/ComponentWrapper.hh"
#include "Engine/Scene/ECSModule/Core.hh"

#include <glm/gtx/quaternion.hpp>
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

auto SceneEntityDB::import_module(this SceneEntityDB &self, flecs::entity module) -> void {
    ZoneScoped;

    self.imported_modules.emplace_back(module);
    module.children([&](flecs::id id) { self.components.push_back(id); });
}

auto SceneEntityDB::is_component_known(this SceneEntityDB &self, flecs::id component_id) -> bool {
    ZoneScoped;

    return std::ranges::any_of(self.components, [&](const auto &id) { return id == component_id; });
}

auto SceneEntityDB::get_components(this SceneEntityDB &self) -> ls::span<flecs::id> {
    return self.components;
}

auto Scene::init(this Scene &self, const std::string &name) -> bool {
    ZoneScoped;

    self.name = name;
    self.world.emplace();
    self.entity_db.import_module(self.world->import <ECS::Core>());
    self.root = self.world->entity();

    self.world
        ->observer<ECS::Transform>()  //
        .event(flecs::Monitor)
        .each([&self](flecs::iter &it, usize i, ECS::Transform &) {
            auto entity = it.entity(i);
            if (it.event() == flecs::OnAdd) {
                auto entity_id = self.add_gpu_entity(entity);
                if (entity.has<ECS::EditorCamera>()) {
                    self.editor_camera_id = entity_id;
                }

                self.dirty_entities.push_back(entity_id);
            } else if (it.event() == flecs::OnRemove) {
                if (auto id = self.get_gpu_entity(entity)) {
                    self.remove_gpu_entity(id.value());
                }
            }
        });

    return true;
}

auto Scene::destroy(this Scene &self) -> void {
    ZoneScoped;

    self.name.clear();
    self.root.clear();
    self.gpu_entities.reset();
    self.gpu_entities_remap.clear();
    self.dirty_entities.clear();
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
        e.set_name(stack.format_char("Entity {}", e.raw_id()));
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

auto Scene::render(this Scene &self, SceneRenderer &renderer, const vuk::Extent3D &extent, vuk::Format format)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto &app = Application::get();
    // clang-format off
    auto camera_query = self.get_world()
        .query_builder<ECS::Transform, ECS::Camera, ECS::ActiveCamera>()
        .build();
    auto model_query = self.get_world()
        .query_builder<ECS::RenderingModel>()
        .build();
    auto directional_light_query = self.get_world()
        .query_builder<ECS::DirectionalLight>()
        .build();
    auto atmosphere_query = self.get_world()
        .query_builder<ECS::Atmosphere>()
        .build();
    // clang-format on

    ls::option<GPUCameraData> active_camera_data = ls::nullopt;
    camera_query.each([&](flecs::entity, ECS::Transform &t, ECS::Camera &c, ECS::ActiveCamera) {
        auto projection_mat = glm::perspective(glm::radians(c.fov), c.aspect_ratio, c.near_clip, c.far_clip);
        projection_mat[1][1] *= -1;

        auto rotation = glm::radians(t.rotation);
        auto yaw_axis = glm::angleAxis(rotation.x, glm::vec3(0.0f, 1.0f, 0.0f));
        auto pitch_axis = glm::angleAxis(rotation.y, glm::vec3(-1.0f, 0.0f, 0.0f));
        auto roll_axis = glm::angleAxis(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        auto orientation = roll_axis * pitch_axis * yaw_axis;

        auto translation_mat = glm::translate(glm::mat4(1.0f), -t.position);
        auto rotation_mat = glm::mat4_cast(orientation);
        auto view_mat = rotation_mat * translation_mat;

        GPUCameraData camera_data = {};
        camera_data.projection_mat = projection_mat;
        camera_data.view_mat = view_mat;
        camera_data.projection_view_mat = glm::transpose(projection_mat * view_mat);
        camera_data.inv_view_mat = glm::inverse(glm::transpose(view_mat));
        camera_data.inv_projection_view_mat = glm::inverse(glm::transpose(projection_mat)) * camera_data.inv_view_mat;
        camera_data.position = t.position;
        camera_data.near_clip = c.near_clip;
        camera_data.far_clip = c.far_clip;
        active_camera_data.emplace(camera_data);
    });

    ls::option<GPUSunData> sun_data = ls::nullopt;
    directional_light_query.each([&](flecs::entity, ECS::DirectionalLight &light) {
        auto rad = glm::radians(light.direction);

        GPUSunData sun = {
            .direction = {
                glm::cos(rad.x) * glm::sin(rad.y),
                glm::sin(rad.x) * glm::sin(rad.y),
                glm::cos(rad.y),
            },
            .intensity = light.intensity
        };
        sun_data.emplace(sun);
    });

    ls::option<GPUAtmosphereData> atmos_data = ls::nullopt;
    atmosphere_query.each([&](flecs::entity, ECS::Atmosphere &atmos_info) {
        GPUAtmosphereData atmos = {};
        atmos.rayleigh_scatter = atmos_info.rayleigh_scattering * 1e-3f;
        atmos.rayleigh_density = atmos_info.rayleigh_density;
        atmos.mie_scatter = atmos_info.mie_scattering * 1e-3f;
        atmos.mie_density = atmos_info.mie_density;
        atmos.mie_extinction = atmos_info.mie_extinction * 1e-3f;
        atmos.ozone_absorption = atmos_info.ozone_absorption * 1e-3f;
        atmos.ozone_height = atmos_info.ozone_height;
        atmos.ozone_thickness = atmos_info.ozone_thickness;
        atmos_data.emplace(atmos);
    });

    std::vector<RenderingMesh> rendering_meshes = {};
    model_query.each([&](flecs::entity e, ECS::RenderingModel &rendering_model) {
        auto *model = app.asset_man.get_model(rendering_model.model);
        if (!model) {
            return;
        }

        auto gpu_entity_id = self.get_gpu_entity(e);
        for (const auto &mesh : model->meshes) {
            for (const auto meshlet_index : mesh.meshlet_indices) {
                const auto &meshlet = model->meshlets[meshlet_index];

                auto &rendering_mesh = rendering_meshes.emplace_back();
                rendering_mesh.entity_index = SlotMap_decode_id(gpu_entity_id.value()).index;

                rendering_mesh.vertex_offset = meshlet.vertex_offset;
                rendering_mesh.index_offset = meshlet.index_offset;
                rendering_mesh.index_count = meshlet.index_count;

                rendering_mesh.vertex_buffer_id = model->vertex_buffer.id();
                rendering_mesh.provoked_index_buffer_id = model->provoked_index_buffer.id();
                rendering_mesh.reordered_index_buffer_id = model->reordered_index_buffer.id();
            }
        }
    });

    for (const auto dirty_entity_id : self.dirty_entities) {
        auto *dirty_entity = self.gpu_entities.slot(dirty_entity_id);
        LS_EXPECT(dirty_entity->has<ECS::Transform>());
        auto *transform = dirty_entity->get_mut<ECS::Transform>();
        auto gpu_entity_info = GPUEntityTransform{};
        auto &matrix = gpu_entity_info.local_transform_mat;

        auto rotation = glm::radians(transform->rotation);
        matrix = glm::translate(glm::mat4(1.0), transform->position);
        matrix *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
        matrix *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
        matrix *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
        matrix *= glm::scale(glm::mat4(1.0), transform->scale);

        if (dirty_entity->has<ECS::RenderingModel>()) {
            // TODO: Split up model nodes into child entities.
        }

        renderer.update_entity_transform(dirty_entity_id, gpu_entity_info);
    }
    self.dirty_entities.clear();

    return renderer.render(SceneRenderInfo{
        .format = format,
        .extent = extent,
        .materials_buffer_id = app.asset_man.material_buffer(),
        .rendering_meshes = std::move(rendering_meshes),
        .camera_info = active_camera_data,
        .sun = sun_data,
        .atmosphere = atmos_data,
    });
}

auto Scene::tick(this Scene &self) -> bool {
    return self.world->progress();
}

auto Scene::set_name(this Scene &self, const std::string &name) -> void {
    ZoneScoped;

    self.name = name;
}

auto Scene::set_dirty(this Scene &self, GPUEntityID gpu_entity_id) -> void {
    ZoneScoped;

    self.dirty_entities.push_back(gpu_entity_id);
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

    return *self.gpu_entities.slot(self.editor_camera_id);
}

auto Scene::get_name(this Scene &self) -> const std::string & {
    ZoneScoped;

    return self.name;
}

auto Scene::get_name_sv(this Scene &self) -> std::string_view {
    ZoneScoped;

    return self.name;
}

auto Scene::get_gpu_entity(this Scene &self, flecs::entity entity) -> ls::option<GPUEntityID> {
    ZoneScoped;

    auto it = self.gpu_entities_remap.find(entity);
    if (it == self.gpu_entities_remap.end()) {
        return ls::nullopt;
    }

    return it->second;
}

auto Scene::get_entity_db(this Scene &self) -> SceneEntityDB & {
    return self.entity_db;
}

auto Scene::add_gpu_entity(this Scene &self, flecs::entity entity) -> GPUEntityID {
    ZoneScoped;

    auto id = self.gpu_entities.create_slot(flecs::entity(entity));
    self.gpu_entities_remap.emplace(entity, id);

    return id;
}

auto Scene::remove_gpu_entity(this Scene &self, GPUEntityID id) -> void {
    ZoneScoped;

    auto *entity = self.gpu_entities.slot(id);
    if (entity) {
        self.gpu_entities_remap.erase(*entity);
        self.gpu_entities.destroy_slot(id);
    }
}

}  // namespace lr
