#include "Engine/Scene/Scene.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Math/Matrix.hh"

#include "Engine/OS/File.hh"

#include "Engine/Scene/GPUScene.hh"
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
        vec[i] = static_cast<T>(o[components[i]].get_double().value_unsafe());
    }

    return true;
}

bool json_to_quat(simdjson::ondemand::value &o, glm::quat &quat) {
    for (i32 i = 0; i < glm::quat::length(); i++) {
        constexpr static std::string_view components[] = { "x", "y", "z", "w" };
        quat[i] = static_cast<f32>(o[components[i]].get_double().value_unsafe());
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
        ->observer<ECS::Transform>() //
        .event(flecs::OnSet)
        .event(flecs::OnAdd)
        .event(flecs::OnRemove)
        .each([&self](flecs::iter &it, usize i, ECS::Transform &) {
            auto entity = it.entity(i);
            if (it.event() == flecs::OnSet) {
                self.set_dirty(entity);
            } else if (it.event() == flecs::OnAdd) {
                self.add_transform(entity);
            } else if (it.event() == flecs::OnRemove) {
                self.remove_transform(entity);
            }
        });

    self.world
        ->observer<ECS::EditorCamera>() //
        .event(flecs::Monitor)
        .each([&self](flecs::iter &it, usize i, ECS::EditorCamera) {
            auto entity = it.entity(i);
            if (it.event() == flecs::OnAdd) {
                self.editor_camera = entity;
            } else if (it.event() == flecs::OnRemove) {
                self.editor_camera.clear();
            }
        });

    self.world
        ->observer<ECS::RenderingModel>() //
        .event(flecs::OnSet)
        .event(flecs::OnRemove)
        .each([&self](flecs::iter &it, usize i, ECS::RenderingModel &rendering_model) {
            if (!rendering_model.uuid) {
                return;
            }

            auto entity = it.entity(i);
            if (it.event() == flecs::OnSet) {
                self.attach_model(entity, rendering_model.uuid);
            } else if (it.event() == flecs::OnRemove) {
                self.detach_model(entity, rendering_model.uuid);
            }
        });

    self.world
        ->system<ECS::Transform, ECS::Camera>() //
        .each([&](flecs::iter &it, usize, ECS::Transform &t, ECS::Camera &c) {
            auto inv_orient = glm::conjugate(Math::compose_quat(glm::radians(t.rotation)));
            t.position += glm::vec3(inv_orient * c.axis_velocity * it.delta_time());

            c.axis_velocity = {};
        });

    return true;
}

auto Scene::destroy(this Scene &self) -> void {
    ZoneScoped;

    {
        auto q = self.world //
                     ->query_builder()
                     .with(flecs::ChildOf, self.root)
                     .build();
        q.each([](flecs::entity e) {
            e.each([&](flecs::id component_id) {
                if (!component_id.is_entity()) {
                    return;
                }

                ECS::ComponentWrapper component(e, component_id);
                if (!component.has_component()) {
                    return;
                }

                component.for_each([&](usize &, std::string_view, ECS::ComponentWrapper::Member &member) {
                    if (auto *component_uuid = std::get_if<UUID *>(&member)) {
                        const auto &uuid = **component_uuid;
                        if (uuid) {
                            auto &app = Application::get();
                            if (uuid && app.asset_man.get_asset(uuid)) {
                                app.asset_man.unload_asset(uuid);
                            }
                        }
                    }
                });
            });
        });
    }

    self.root.destruct();
    self.name.clear();
    self.root.clear();
    self.transforms.reset();
    self.entity_transforms_map.clear();
    self.dirty_transforms.clear();
    self.rendering_models.clear();
    self.world.reset();
}

static auto json_to_entity(Scene &self, flecs::entity root, simdjson::ondemand::value &json, std::vector<UUID> &requested_assets) -> bool {
    ZoneScoped;
    memory::ScopedStack stack;

    auto &world = self.get_world();

    auto entity_name_json = json["name"];
    if (entity_name_json.error()) {
        LOG_ERROR("Entities must have names!");
        return false;
    }

    auto entity_name = entity_name_json.get_string().value_unsafe();
    auto e = self.create_entity(std::string(entity_name.begin(), entity_name.end()));
    e.child_of(root);

    auto entity_tags_json = json["tags"];
    for (auto entity_tag : entity_tags_json.get_array()) {
        auto tag = world.component(stack.null_terminate(entity_tag.get_string().value_unsafe()).data());
        e.add(tag);
    }

    auto components_json = json["components"];
    for (auto component_json : components_json.get_array()) {
        auto component_name_json = component_json["name"];
        if (component_name_json.error()) {
            LOG_ERROR("Entity '{}' has corrupt components JSON array.", e.name());
            return false;
        }

        const auto *component_name = stack.null_terminate_cstr(component_name_json.get_string().value_unsafe());
        auto component_id = world.lookup(component_name);
        if (!component_id) {
            LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name(), component_name);
            return false;
        }

        LS_EXPECT(self.get_entity_db().is_component_known(component_id));
        e.add(component_id);
        ECS::ComponentWrapper component(e, component_id);
        component.for_each([&](usize &, std::string_view member_name, ECS::ComponentWrapper::Member &member) {
            auto member_json = component_json[member_name];
            if (member_json.error()) {
                // Default construct
                return;
            }

            std::visit(
                ls::match{
                    [](const auto &) {},
                    [&](f32 *v) { *v = static_cast<f32>(member_json.get_double().value_unsafe()); },
                    [&](i32 *v) { *v = static_cast<i32>(member_json.get_int64().value_unsafe()); },
                    [&](u32 *v) { *v = member_json.get_uint64().value_unsafe(); },
                    [&](i64 *v) { *v = member_json.get_int64().value_unsafe(); },
                    [&](u64 *v) { *v = member_json.get_uint64().value_unsafe(); },
                    [&](glm::vec2 *v) { json_to_vec(member_json.value_unsafe(), *v); },
                    [&](glm::vec3 *v) { json_to_vec(member_json.value_unsafe(), *v); },
                    [&](glm::vec4 *v) { json_to_vec(member_json.value_unsafe(), *v); },
                    [&](glm::quat *v) { json_to_quat(member_json.value_unsafe(), *v); },
                    // [&](glm::mat4 *v) {json_to_mat(member_json.value(), *v); },
                    [&](std::string *v) { *v = member_json.get_string().value_unsafe(); },
                    [&](UUID *v) {
                        *v = UUID::from_string(member_json.get_string().value_unsafe()).value();
                        requested_assets.push_back(*v);
                    },
                },
                member
            );
        });

        e.modified(component_id);
    }

    auto children_json = json["children"];
    for (auto children : children_json) {
        if (children.error()) {
            continue;
        }

        if (!json_to_entity(self, e, children.value_unsafe(), requested_assets)) {
            return false;
        }
    }

    return true;
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

    self.set_name(std::string(name_json.value_unsafe()));

    std::vector<UUID> requested_assets = {};
    auto entities_json = doc["entities"].get_array();
    for (auto entity_json : entities_json) {
        if (entity_json.error()) {
            continue;
        }

        if (!json_to_entity(self, self.root, entity_json.value_unsafe(), requested_assets)) {
            return false;
        }
    }

    LOG_TRACE("Loading scene {} with {} assets...", self.name, requested_assets.size());
    for (const auto &uuid : requested_assets) {
        auto &app = Application::get();
        if (uuid && app.asset_man.get_asset(uuid)) {
            app.asset_man.load_asset(uuid);
        }
    }

    return true;
}

auto entity_to_json(JsonWriter &json, flecs::entity root) -> void {
    ZoneScoped;

    auto q = root.world() //
                 .query_builder()
                 .with(flecs::ChildOf, root)
                 .build();

    q.each([&](flecs::entity e) {
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
                    ls::match{
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
                    member
                );
            });
            json.end_obj();
        }
        json.end_array();

        json["children"].begin_array();
        entity_to_json(json, e);
        json.end_array();

        json.end_obj();
    });
}

auto Scene::export_to_file(this Scene &self, const fs::path &path) -> bool {
    ZoneScoped;

    JsonWriter json;
    json.begin_obj();
    json["name"] = self.name;
    json["entities"].begin_array();
    entity_to_json(json, self.root);
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

        auto e = self.world->entity();
        e.set_name(stack.format_char("Entity {}", e.raw_id()));
        return e;
    }

    return self.world->entity(name_sv);
}

auto Scene::create_perspective_camera(
    this Scene &self,
    const std::string &name,
    const glm::vec3 &position,
    const glm::vec3 &rotation,
    f32 fov,
    f32 aspect_ratio
) -> flecs::entity {
    ZoneScoped;

    return self
        .create_entity(name) //
        .add<ECS::PerspectiveCamera>()
        .set<ECS::Transform>({ .position = position, .rotation = Math::normalize_180(rotation) })
        .set<ECS::Camera>({ .fov = fov, .aspect_ratio = aspect_ratio });
}

auto Scene::create_editor_camera(this Scene &self) -> void {
    ZoneScoped;

    self.create_perspective_camera("editor_camera", { 0.0, 2.0, 0.0 }, { 0, 0, 0 }, 65.0, 1.6)
        .add<ECS::Hidden>()
        .add<ECS::EditorCamera>()
        .add<ECS::ActiveCamera>()
        .child_of(self.root);
}

auto Scene::find_entity(this Scene &self, std::string_view name) -> flecs::entity {
    ZoneScoped;
    memory::ScopedStack stack;

    const auto *safe_str = stack.null_terminate_cstr(name);
    return self.root.lookup(safe_str);
}

auto Scene::find_entity(this Scene &self, u32 transform_index) -> flecs::entity {
    ZoneScoped;

    for (const auto &[entity, transform_id] : self.entity_transforms_map) {
        auto i = SlotMap_decode_id(transform_id).index;
        if (i == transform_index) {
            return entity;
        }
    }

    return {};
}

auto Scene::render(this Scene &self, SceneRenderer &renderer, SceneRenderInfo &info) -> vuk::Value<vuk::ImageAttachment> {
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
    // clang-format on

    ls::option<GPU::Camera> active_camera_data = ls::nullopt;
    camera_query.each([&](flecs::entity, ECS::Transform &t, ECS::Camera &c, ECS::ActiveCamera) {
        auto projection_mat = glm::perspective(glm::radians(c.fov), c.aspect_ratio, c.near_clip, c.far_clip);
        projection_mat[1][1] *= -1;

        auto translation_mat = glm::translate(glm::mat4(1.0f), -t.position);
        auto rotation_mat = glm::mat4_cast(Math::compose_quat(glm::radians(t.rotation)));
        auto view_mat = rotation_mat * translation_mat;

        auto &camera_data = active_camera_data.emplace();
        camera_data.projection_mat = projection_mat;
        camera_data.view_mat = view_mat;
        camera_data.projection_view_mat = camera_data.projection_mat * camera_data.view_mat;
        camera_data.inv_view_mat = glm::inverse(camera_data.view_mat);
        camera_data.inv_projection_view_mat = glm::inverse(camera_data.projection_view_mat);
        camera_data.position = t.position;
        camera_data.near_clip = c.near_clip;
        camera_data.far_clip = c.far_clip;
        camera_data.resolution = glm::vec2(static_cast<f32>(info.extent.width), static_cast<f32>(info.extent.height));

        if (!c.freeze_frustum) {
            c.frustum_projection_view_mat = camera_data.projection_view_mat;
            camera_data.frustum_projection_view_mat = camera_data.projection_view_mat;
        } else {
            camera_data.frustum_projection_view_mat = c.frustum_projection_view_mat;
        }
        Math::calc_frustum_planes(camera_data.frustum_projection_view_mat, camera_data.frustum_planes);
    });

    ls::option<GPU::Sun> sun_data = ls::nullopt;
    ls::option<GPU::Atmosphere> atmos_data = ls::nullopt;
    ls::option<GPU::HistogramInfo> histogram_data = ls::nullopt;
    directional_light_query.each([&](flecs::entity e, ECS::DirectionalLight &light) {
        auto light_dir_rad = glm::radians(light.direction);

        auto &sun = sun_data.emplace();
        sun.direction.x = glm::cos(light_dir_rad.x) * glm::sin(light_dir_rad.y);
        sun.direction.y = glm::sin(light_dir_rad.x) * glm::sin(light_dir_rad.y);
        sun.direction.z = glm::cos(light_dir_rad.y);
        sun.intensity = light.intensity;

        if (e.has<ECS::Atmosphere>()) {
            const auto &atmos_info = *e.get<ECS::Atmosphere>();
            auto &atmos = atmos_data.emplace();
            atmos.rayleigh_scatter = atmos_info.rayleigh_scattering * 1e-3f;
            atmos.rayleigh_density = atmos_info.rayleigh_density;
            atmos.mie_scatter = atmos_info.mie_scattering * 1e-3f;
            atmos.mie_density = atmos_info.mie_density;
            atmos.mie_extinction = atmos_info.mie_extinction * 1e-3f;
            atmos.mie_asymmetry = atmos_info.mie_asymmetry;
            atmos.ozone_absorption = atmos_info.ozone_absorption * 1e-3f;
            atmos.ozone_height = atmos_info.ozone_height;
            atmos.ozone_thickness = atmos_info.ozone_thickness;
            atmos.aerial_gain_per_slice = atmos_info.aerial_gain_per_slice;
        }

        if (e.has<ECS::AutoExposure>()) {
            const auto &auto_exposure = *e.get<ECS::AutoExposure>();
            auto &histogram = histogram_data.emplace();
            histogram.min_exposure = auto_exposure.min_exposure;
            histogram.max_exposure = auto_exposure.max_exposure;
            histogram.adaptation_speed = auto_exposure.adaptation_speed;
            histogram.ev100_bias = auto_exposure.ev100_bias;
        }
    });

    ls::option<ComposedScene> composed_scene = ls::nullopt;
    if (self.models_dirty) {
        memory::ScopedStack stack;
        self.models_dirty = false;

        auto compose_info = self.compose();
        composed_scene.emplace(renderer.compose(compose_info));
    }

    info.materials_descriptor_set = app.asset_man.get_materials_descriptor_set();
    info.materials_buffer = app.asset_man.get_materials_buffer();
    info.sun = sun_data;
    info.atmosphere = atmos_data;
    info.camera = active_camera_data;
    info.histogram_info = histogram_data;
    info.cull_flags = self.cull_flags;
    info.dirty_transform_ids = self.dirty_transforms;
    info.transforms = self.transforms.slots_unsafe();
    auto rendered_attachment = renderer.render(info, composed_scene);
    self.dirty_transforms.clear();

    return rendered_attachment;
}

auto Scene::tick(this Scene &self, f32 delta_time) -> bool {
    return self.world->progress(delta_time);
}

auto Scene::set_name(this Scene &self, const std::string &name) -> void {
    ZoneScoped;

    self.name = name;
}

auto Scene::set_dirty(this Scene &self, flecs::entity entity) -> void {
    ZoneScoped;

    LS_EXPECT(entity.has<ECS::Transform>());
    auto it = self.entity_transforms_map.find(entity);
    if (it == self.entity_transforms_map.end()) {
        return;
    }

    auto transform_id = it->second;
    const auto *entity_transform = entity.get<ECS::Transform>();
    auto *gpu_transform = self.transforms.slot(transform_id);

    const auto &rotation = glm::radians(entity_transform->rotation);
    gpu_transform->local = glm::mat4(1.0);
    gpu_transform->world = glm::translate(glm::mat4(1.0), entity_transform->position);
    gpu_transform->world *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
    gpu_transform->world *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
    gpu_transform->world *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
    gpu_transform->world *= glm::scale(glm::mat4(1.0), entity_transform->scale);
    gpu_transform->normal = glm::mat3(gpu_transform->world);

    self.dirty_transforms.push_back(transform_id);
}

auto Scene::get_root(this Scene &self) -> flecs::entity {
    ZoneScoped;

    return self.root;
}

auto Scene::get_world(this Scene &self) -> flecs::world & {
    ZoneScoped;

    return self.world.value();
}

auto Scene::get_editor_camera(this Scene &self) -> flecs::entity {
    ZoneScoped;

    return self.editor_camera;
}

auto Scene::get_name(this Scene &self) -> const std::string & {
    ZoneScoped;

    return self.name;
}

auto Scene::get_name_sv(this Scene &self) -> std::string_view {
    ZoneScoped;

    return self.name;
}

auto Scene::get_entity_db(this Scene &self) -> SceneEntityDB & {
    return self.entity_db;
}

auto Scene::get_cull_flags(this Scene &self) -> GPU::CullFlags & {
    return self.cull_flags;
}

auto Scene::compose(this Scene &self) -> SceneComposeInfo {
    ZoneScoped;

    auto &app = Application::get();

    auto rendering_image_view_ids = std::vector<ImageViewID>();
    auto gpu_models = std::vector<GPU::Model>();
    auto gpu_meshlet_instances = std::vector<GPU::MeshletInstance>();

    for (const auto &[model_uuid, transform_ids] : self.rendering_models) {
        auto *model = app.asset_man.get_model(model_uuid);

        //  ── PER MODEL INFORMATION ───────────────────────────────────────────
        auto model_offset = gpu_models.size();
        auto &gpu_model = gpu_models.emplace_back();
        gpu_model.indices = model->indices.device_address();
        gpu_model.vertex_positions = model->vertex_positions.device_address();
        gpu_model.vertex_normals = model->vertex_normals.device_address();
        gpu_model.texture_coords = model->texture_coords.device_address();
        gpu_model.meshlets = model->meshlets.device_address();
        gpu_model.meshlet_bounds = model->meshlet_bounds.device_address();
        gpu_model.local_triangle_indices = model->local_triangle_indices.device_address();

        //  ── INSTANCING ──────────────────────────────────────────────────────
        for (const auto transform_id : transform_ids) {
            u32 meshlet_offset = 0;
            for (const auto &primitive : model->primitives) {
                for (u32 meshlet_index = 0; meshlet_index < primitive.meshlet_count; meshlet_index++) {
                    auto &meshlet_instance = gpu_meshlet_instances.emplace_back();
                    meshlet_instance.model_index = model_offset;
                    meshlet_instance.material_index = primitive.material_index;
                    meshlet_instance.transform_index = SlotMap_decode_id(transform_id).index;
                    meshlet_instance.meshlet_index = meshlet_index + meshlet_offset;
                }

                meshlet_offset += primitive.meshlet_count;
            }
        }
    }

    return SceneComposeInfo{
        .rendering_image_view_ids = std::move(rendering_image_view_ids),
        .gpu_models = std::move(gpu_models),
        .gpu_meshlet_instances = std::move(gpu_meshlet_instances),
    };
}

auto Scene::add_transform(this Scene &self, flecs::entity entity) -> GPU::TransformID {
    ZoneScoped;

    auto id = self.transforms.create_slot();
    self.entity_transforms_map.emplace(entity, id);

    return id;
}

auto Scene::remove_transform(this Scene &self, flecs::entity entity) -> void {
    ZoneScoped;

    auto it = self.entity_transforms_map.find(entity);
    if (it == self.entity_transforms_map.end()) {
        return;
    }

    self.transforms.destroy_slot(it->second);
    self.entity_transforms_map.erase(it);
}

auto Scene::attach_model(this Scene &self, flecs::entity entity, const UUID &model_uuid) -> bool {
    ZoneScoped;

    auto transforms_it = self.entity_transforms_map.find(entity);
    if (transforms_it == self.entity_transforms_map.end()) {
        // Target entity must have a transform component, figure out
        // why its missing.
        LS_DEBUGBREAK();
        return false;
    }

    const auto transform_id = transforms_it->second;
    // Find the old model UUID and detach it from entity.
    // TODO: This is retarded
    auto old_model_uuid = UUID(nullptr);
    for (const auto &[cur_old_model_uuid, transform_ids] : self.rendering_models) {
        if (std::ranges::find(transform_ids, transform_id) != transform_ids.end()) {
            old_model_uuid = cur_old_model_uuid;
            break;
        }
    }
    if (old_model_uuid) {
        self.detach_model(entity, old_model_uuid);
    }

    auto instances_it = self.rendering_models.find(model_uuid);
    if (instances_it == self.rendering_models.end()) {
        bool inserted = false;
        std::tie(instances_it, inserted) = self.rendering_models.try_emplace(model_uuid);
        if (!inserted) {
            return false;
        }
    }

    auto &instances = instances_it->second;
    instances.push_back(transform_id);
    self.models_dirty = true;
    self.set_dirty(entity);

    return true;
}

auto Scene::detach_model(this Scene &self, flecs::entity entity, const UUID &model_uuid) -> bool {
    ZoneScoped;

    auto instances_it = self.rendering_models.find(model_uuid);
    auto transforms_it = self.entity_transforms_map.find(entity);
    if (instances_it == self.rendering_models.end() || transforms_it == self.entity_transforms_map.end()) {
        return false;
    }

    const auto transform_id = transforms_it->second;
    auto &instances = instances_it->second;
    std::erase_if(instances, [transform_id](const GPU::TransformID &id) { return id == transform_id; });
    self.models_dirty = true;

    if (instances.empty()) {
        self.rendering_models.erase(instances_it);
    }

    return true;
}

} // namespace lr
