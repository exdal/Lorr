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
        ->observer<ECS::EditorCamera>()  //
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
        ->observer<ECS::RenderingModel>()  //
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
                self.detach_model(entity);
            }
        });

    self.world
        ->system<ECS::Transform, ECS::Camera>()  //
        .each([&](flecs::iter &it, usize, ECS::Transform &t, ECS::Camera &c) {
            auto inv_orient = glm::conjugate(Math::compose_quat(glm::radians(t.rotation)));
            t.position += glm::vec3(inv_orient * c.axis_velocity * it.delta_time());

            c.axis_velocity = {};
        });

    return true;
}

auto Scene::destroy(this Scene &self) -> void {
    ZoneScoped;

    self.name.clear();
    self.root.clear();
    self.entity_transforms.reset();
    self.entity_transforms_map.clear();
    self.dirty_transforms.clear();
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

            LS_EXPECT(self.entity_db.is_component_known(component_id));
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

            e.modified(component_id);
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
        .set<ECS::Transform>({ .position = position, .rotation = Math::normalize_180(rotation) })
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
    });

    ls::option<GPU::Sun> sun_data = ls::nullopt;
    ls::option<GPU::Atmosphere> atmos_data = ls::nullopt;
    ls::option<GPU::Clouds> clouds_data = ls::nullopt;
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
            atmos.ozone_absorption = atmos_info.ozone_absorption * 1e-3f;
            atmos.ozone_height = atmos_info.ozone_height;
            atmos.ozone_thickness = atmos_info.ozone_thickness;
        }

        if (e.has<ECS::Clouds>()) {
            const auto &clouds_info = *e.get<ECS::Clouds>();
            auto &clouds = clouds_data.emplace();
            clouds.bounds = clouds_info.bounds;
            clouds.shape_noise_scale = clouds_info.shape_noise_scale;
            clouds.shape_noise_weights = clouds_info.shape_noise_weights;
            clouds.detail_noise_scale = clouds_info.detail_noise_scale;
            clouds.detail_noise_weights = clouds_info.detail_noise_weights;
            clouds.detail_noise_influence = clouds_info.detail_noise_influence;
            clouds.coverage = clouds_info.coverage;
            clouds.general_density = clouds_info.general_density;
            clouds.phase_values = clouds_info.phase_values;
            clouds.extinction = clouds_info.extinction;
            clouds.scattering = clouds_info.scattering;
            clouds.clouds_step_count = clouds_info.clouds_step_count;
            clouds.sun_step_count = clouds_info.sun_step_count;
            clouds.darkness_threshold = clouds_info.darkness_threshold;
            clouds.draw_distance = clouds_info.draw_distance;
            clouds.cloud_type = clouds_info.cloud_type;
            clouds.powder_intensity = clouds_info.powder_intensity;
        }
    });

    ls::option<ComposedScene> composed_scene = ls::nullopt;
    if (self.models_dirty) {
        memory::ScopedStack stack;
        self.models_dirty = false;

        auto compose_info = self.compose();
        composed_scene.emplace(renderer.compose(compose_info));
    }

    auto transforms = self.entity_transforms.slots_unsafe();

    auto scene_info = GPU::Scene{
        .camera = active_camera_data.value(),
        .sun = sun_data.value_or(GPU::Sun{}),
        .atmosphere = atmos_data.value_or(GPU::Atmosphere{}),
        .clouds = clouds_data.value_or(GPU::Clouds{}),
    };

    auto render_info = SceneRenderInfo{
        .format = format,
        .extent = extent,
        .scene_info = scene_info,
        .dirty_transform_ids = self.dirty_transforms,
        .transforms = transforms,
        .render_atmos = sun_data.has_value() && atmos_data.has_value(),
        .render_clouds = clouds_data.has_value(),
    };

    auto rendered_attachment = renderer.render(render_info, composed_scene);
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
    auto *gpu_transform = self.entity_transforms.slot(transform_id);

    auto &local_transform = gpu_transform->local;
    const auto &rotation = glm::radians(entity_transform->rotation);
    local_transform = glm::translate(glm::mat4(1.0), entity_transform->position);
    local_transform *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
    local_transform *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
    local_transform *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
    local_transform *= glm::scale(glm::mat4(1.0), entity_transform->scale);

    self.dirty_transforms.push_back(transform_id);
}

auto Scene::attach_model(this Scene &self, flecs::entity entity, const UUID &model_uuid) -> bool {
    ZoneScoped;

    if (!entity.has<ECS::RenderingModel>()) {
        return false;
    }

    auto *rendering_model = entity.get_mut<ECS::RenderingModel>();
    if (!rendering_model) {
        return false;
    }

    self.detach_model(entity);

    auto transforms_it = self.entity_transforms_map.find(entity);
    if (transforms_it == self.entity_transforms_map.end()) {
        // Target entity must have a transform component, figure out why its missing.
        LS_DEBUGBREAK();
        return false;
    }

    auto instances_it = self.rendering_models.find(model_uuid);
    if (instances_it == self.rendering_models.end()) {
        bool inserted = false;
        std::tie(instances_it, inserted) = self.rendering_models.try_emplace(model_uuid);
        if (!inserted) {
            return false;
        }
    }

    const auto transform_id = transforms_it->second;
    auto &instances = instances_it->second;
    instances.push_back(transform_id);
    self.models_dirty = true;

    return true;
}

auto Scene::detach_model(this Scene &self, flecs::entity entity) -> bool {
    ZoneScoped;

    if (!entity.has<ECS::RenderingModel>()) {
        return false;
    }

    auto *rendering_model = entity.get_mut<ECS::RenderingModel>();
    if (!rendering_model) {
        return false;
    }

    auto instances_it = self.rendering_models.find(rendering_model->uuid);
    auto transforms_it = self.entity_transforms_map.find(entity);
    if (instances_it == self.rendering_models.end() || transforms_it == self.entity_transforms_map.end()) {
        return false;
    }

    const auto transform_id = transforms_it->second;
    auto &instances = instances_it->second;
    std::erase_if(instances, [transform_id](const GPU::TransformID id) { return id == transform_id; });
    self.models_dirty = true;

    if (instances.empty()) {
        self.rendering_models.erase(instances_it);
    }

    return true;
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

auto Scene::compose(this Scene &self) -> SceneComposeInfo {
    ZoneScoped;

    auto &app = Application::get();

    auto gpu_models = std::vector<GPU::Model>();
    auto gpu_meshlet_instances = std::vector<GPU::MeshletInstance>();
    auto gpu_materials = std::vector<GPU::Material>();
    auto gpu_image_views = std::vector<ImageViewID>();
    auto gpu_samplers = std::vector<SamplerID>();

    for (const auto &[model_uuid, transform_ids] : self.rendering_models) {
        auto *model = app.asset_man.get_model(model_uuid);

        //  ── PER MODEL INFORMATION ───────────────────────────────────────────
        auto model_offset = gpu_models.size();
        auto &gpu_model = gpu_models.emplace_back();
        gpu_model.vertex_positions = model->vertex_positions.device_address();
        gpu_model.indices = model->indices.device_address();
        gpu_model.texture_coords = model->texture_coords.device_address();
        gpu_model.meshlets = model->meshlets.device_address();
        gpu_model.meshlet_bounds = model->meshlet_bounds.device_address();
        gpu_model.local_triangle_indices = model->local_triangle_indices.device_address();

        auto material_offset = gpu_materials.size();
        for (const auto &material_uuid : model->materials) {
            auto *material = app.asset_man.get_material(material_uuid);
            auto &gpu_material = gpu_materials.emplace_back();
            gpu_material.albedo_color = material->albedo_color;
            gpu_material.emissive_color = material->emissive_color;
            gpu_material.roughness_factor = material->roughness_factor;
            gpu_material.metallic_factor = material->metallic_factor;
            gpu_material.alpha_cutoff = material->alpha_cutoff;

            auto add_image_if_exists = [&](const UUID &uuid) -> ls::option<u32> {
                if (!uuid) {
                    return ls::nullopt;
                }

                u32 index = gpu_image_views.size();
                auto *texture = app.asset_man.get_texture(uuid);
                gpu_image_views.emplace_back(texture->image_view.id());
                gpu_samplers.emplace_back(texture->sampler.id());
                return index;
            };

            gpu_material.albedo_image_index = add_image_if_exists(material->albedo_texture).value_or(~0_u32);
            gpu_material.normal_image_index = add_image_if_exists(material->normal_texture).value_or(~0_u32);
            gpu_material.emissive_image_index = add_image_if_exists(material->emissive_texture).value_or(~0_u32);
        }

        //  ── INSTANCING ──────────────────────────────────────────────────────
        for (const auto transform_id : transform_ids) {
            u32 meshlet_offset = 0;
            for (const auto &primitive : model->primitives) {
                for (u32 meshlet_index = 0; meshlet_index < primitive.meshlet_count; meshlet_index++) {
                    auto &meshlet_instance = gpu_meshlet_instances.emplace_back();
                    meshlet_instance.model_index = model_offset;
                    meshlet_instance.material_index = material_offset + primitive.material_index;
                    meshlet_instance.transform_index = SlotMap_decode_id(transform_id).index;
                    meshlet_instance.meshlet_index = meshlet_index + meshlet_offset;
                }

                meshlet_offset += primitive.meshlet_count;
            }
        }
    }

    return SceneComposeInfo{
        .image_view_ids = std::move(gpu_image_views),
        .samplers = std::move(gpu_samplers),
        .gpu_materials = std::move(gpu_materials),
        .gpu_models = std::move(gpu_models),
        .gpu_meshlet_instances = std::move(gpu_meshlet_instances),
    };
}

auto Scene::add_transform(this Scene &self, flecs::entity entity) -> GPU::TransformID {
    ZoneScoped;

    auto id = self.entity_transforms.create_slot();
    self.entity_transforms_map.emplace(entity, id);

    return id;
}

auto Scene::remove_transform(this Scene &self, flecs::entity entity) -> void {
    ZoneScoped;

    auto it = self.entity_transforms_map.find(entity);
    if (it == self.entity_transforms_map.end()) {
        return;
    }

    self.entity_transforms.destroy_slot(it->second);
    self.entity_transforms_map.erase(it);
}

}  // namespace lr
