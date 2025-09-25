#include "Engine/Scene/Scene.hh"

#include "Engine/Asset/Asset.hh"

#include "Engine/Core/App.hh"

#include "Engine/Math/Quat.hh"
#include "Engine/Memory/Stack.hh"

#include "Engine/OS/File.hh"

#include "Engine/Scene/GPUScene.hh"
#include "Engine/Util/JsonWriter.hh"

#include "Engine/Scene/ECSModule/ComponentWrapper.hh"
#include "Engine/Scene/ECSModule/Core.hh"

#include <glm/gtx/matrix_decompose.hpp>
#include <simdjson.h>

#include <queue>

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

auto calculate_virtual_shadow_matrices(
    GPU::DirectionalLight &light,
    ls::span<glm::mat4> clipmap_projection_view_mats,
    const ECS::DirectionalLight &light_comp,
    const GPU::Camera &camera
) -> void {
    ZoneScoped;

    // * Stable matrices *
    // We want our directional light matrix to be "stable" aka positionless.
    // This is to not keep getting cache hits on the virtual pages each time
    // camera moves. Later, we will offset the resulting projection view mat
    // per each page.

    auto forward = glm::normalize(-light.direction);
    auto up = (glm::abs(glm::dot(forward, glm::vec3(0, 1, 0))) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    auto world_from_light = glm::lookAt(forward, glm::vec3(0.0f), up);

    for (u32 clipmap_index = 0; clipmap_index < light.clipmap_count; clipmap_index++) {
        auto clipmap_extent = light_comp.first_clipmap_width * static_cast<f32>(1 << clipmap_index);
        auto clipmap_half_extent = clipmap_extent * 0.5f;

        auto z_extension = camera.far_clip * 0.5f;
        auto clip_from_clipmap = glm::ortho(
            -clipmap_half_extent, //
            clipmap_half_extent,
            -clipmap_half_extent,
            clipmap_half_extent,
            -z_extension,
            z_extension
        );

        // Offset projection to page
        auto page_table_size = static_cast<f32>(GPU::VSM_PAGE_TABLE_SIZE);
        auto clip_position = clip_from_clipmap * world_from_light * glm::vec4(camera.position, 1.0f);
        auto ndc_position = glm::vec2(clip_position) / clip_position.w;
        auto center_uv_position = ndc_position * 0.5f;
        auto page_offset = glm::ivec2(center_uv_position * glm::vec2(page_table_size));
        auto page_shift = (glm::vec2(page_offset) / glm::vec2(page_table_size)) * 2.0f;
        auto shifted_projection_mat = glm::translate(glm::mat4(1.0f), glm::vec3(-page_shift, 0.0f)) * clip_from_clipmap;
        auto clipmap_from_page = glm::inverse(clip_from_clipmap) * shifted_projection_mat * world_from_light;

        clipmap_projection_view_mats[clipmap_index] = clip_from_clipmap * clipmap_from_page;

        fmt::println("clipmap{}: uv = {}, {}", clipmap_index, center_uv_position.x, center_uv_position.y);
    }
}

auto Scene::init(this Scene &self, const std::string &name) -> bool {
    ZoneScoped;

    self.name = name;
    self.world.emplace();
    self.import_module<ECS::Core>();

    self.world->observer<ECS::Transform>()
        .event(flecs::OnSet)
        .event(flecs::OnAdd)
        .event(flecs::OnRemove)
        .each([&self](flecs::iter &it, usize i, ECS::Transform &) {
            auto entity = it.entity(i);
            auto event = it.event();
            if (event == flecs::OnSet) {
                self.set_dirty(entity);
            } else if (event == flecs::OnAdd) {
                self.add_transform(entity);
                self.set_dirty(entity);
            } else if (event == flecs::OnRemove) {
                self.remove_transform(entity);
            }
        });

    self.world->observer<ECS::RenderingMesh>()
        .event(flecs::OnSet)
        .event(flecs::OnRemove)
        .each([&self](flecs::iter &it, usize i, ECS::RenderingMesh &mesh) {
            if (!mesh.model_uuid) {
                return;
            }

            auto entity = it.entity(i);
            auto event = it.event();
            if (event == flecs::OnSet) {
                self.attach_mesh(entity, mesh.model_uuid, mesh.mesh_index);
            } else if (event == flecs::OnRemove) {
                self.detach_mesh(entity, mesh.model_uuid, mesh.mesh_index);
            }
        });

    self.root = self.world->entity();
    self.root.add<ECS::Transform>();

    return true;
}

auto Scene::destroy(this Scene &self) -> void {
    ZoneScoped;

    auto unloading_assets = std::vector<UUID>();

    auto visit_child = [&](this auto &visitor, flecs::entity &e) -> void {
        e.each([&](flecs::id component_id) {
            if (!component_id.is_entity()) {
                return;
            }

            ECS::ComponentWrapper component(e, component_id);
            component.for_each([&](usize &, std::string_view, ECS::ComponentWrapper::Member &member) {
                if (auto *component_uuid = std::get_if<UUID *>(&member)) {
                    const auto &uuid = **component_uuid;
                    unloading_assets.push_back(uuid);
                }
            });
        });

        e.children([&](flecs::entity child) { visitor(child); });
    };
    self.root.children([&](flecs::entity e) { visit_child(e); });

    auto &asset_man = App::mod<AssetManager>();
    for (const auto &uuid : unloading_assets) {
        if (uuid && asset_man.get_asset(uuid)) {
            asset_man.unload_asset(uuid);
        }
    }

    self.mesh_instance_count = 0;
    self.max_meshlet_instance_count = 0;
    self.root.destruct();
    self.name.clear();
    self.root.clear();
    self.transforms.reset();
    self.entity_transforms_map.clear();
    self.dirty_transforms.clear();
    self.rendering_meshes_map.clear();
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
            continue;
        }

        LS_EXPECT(self.is_component_known(component_id));

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
                    [&](bool *v) { *v = member_json.get_bool().value_unsafe(); },
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

auto Scene::import_module(this Scene &self, flecs::entity module_entity) -> void {
    ZoneScoped;

    module_entity.children([&](flecs::id id) { self.known_component_ids.push_back(id); });
}

auto Scene::is_component_known(this Scene &self, flecs::id component_id) -> bool {
    ZoneScoped;

    return std::ranges::any_of(self.known_component_ids, [&](const auto &id) { return id == component_id; });
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
        auto &asset_man = App::mod<AssetManager>();
        if (uuid && asset_man.get_asset(uuid)) {
            asset_man.load_asset(uuid);
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
            if (!component.is_component()) {
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
                        [&](bool *v) { member_json = *v; },
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

auto Scene::delete_entity(this Scene &, flecs::entity entity) -> void {
    ZoneScoped;

    entity.destruct();
}

auto Scene::create_perspective_camera(this Scene &self, const std::string &name, const glm::vec3 &position, f32 yaw, f32 pitch, f32 fov)
    -> flecs::entity {
    ZoneScoped;

    return self
        .create_entity(name) //
        .add<ECS::PerspectiveCamera>()
        .set<ECS::Transform>({ .position = position, .rotation = { yaw, pitch, 0.0f } })
        .set<ECS::Camera>({ .fov = fov })
        .child_of(self.root);
}

auto Scene::create_model_entity(this Scene &self, UUID &importing_model_uuid) -> flecs::entity {
    ZoneScoped;

    auto &asset_man = App::mod<AssetManager>();

    // sanity check
    if (!asset_man.get_asset(importing_model_uuid)) {
        LOG_ERROR("Cannot import an invalid model '{}' into the scene!", importing_model_uuid.str());
        return {};
    }

    // acquire model
    if (!asset_man.load_model(importing_model_uuid)) {
        return {};
    }

    auto *imported_model = asset_man.get_model(importing_model_uuid);
    auto &default_scene = imported_model->scenes[imported_model->default_scene_index];
    auto root_entity = self.create_entity(self.find_entity(default_scene.name) ? std::string{} : default_scene.name);
    root_entity.child_of(self.root);
    root_entity.add<ECS::Transform>();

    auto visit_nodes = [&](this auto &visitor, flecs::entity &root, std::vector<usize> &node_indices) -> void {
        for (const auto node_index : node_indices) {
            auto &cur_node = imported_model->nodes[node_index];
            auto node_entity = self.create_entity();

            const auto T = glm::translate(glm::mat4(1.0f), cur_node.translation);
            const auto R = glm::mat4_cast(cur_node.rotation);
            const auto S = glm::scale(glm::mat4(1.0f), cur_node.scale);
            auto TRS = T * R * S;
            auto transform_comp = ECS::Transform{};
            {
                glm::quat rotation = {};
                glm::vec3 skew = {};
                glm::vec4 perspective = {};
                glm::decompose(TRS, transform_comp.scale, rotation, transform_comp.position, skew, perspective);
                transform_comp.rotation = glm::eulerAngles(glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]));
            }
            node_entity.set(transform_comp);

            if (cur_node.mesh_index.has_value()) {
                node_entity.set<ECS::RenderingMesh>({
                    .model_uuid = importing_model_uuid,
                    .mesh_index = static_cast<u32>(cur_node.mesh_index.value()),
                });
            }

            node_entity.child_of(root);
            node_entity.modified<lr::ECS::Transform>();

            visitor(node_entity, cur_node.child_indices);
        }
    };

    visit_nodes(root_entity, default_scene.node_indices);

    return root_entity;
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
    auto bfs_stack = std::queue<flecs::entity>();
    bfs_stack.push(entity);

    while (!bfs_stack.empty()) {
        auto cur_entity = bfs_stack.front();
        bfs_stack.pop();

        const auto *entity_transform = cur_entity.get<ECS::Transform>();
        const auto T = glm::translate(glm::mat4(1.0), entity_transform->position);
        const auto R = glm::mat4_cast(Math::quat_dir(entity_transform->rotation));
        const auto S = glm::scale(glm::mat4(1.0), entity_transform->scale);
        auto local_mat = T * R * S;
        auto world_mat = local_mat;

        auto parent_entity = cur_entity.parent();
        if (parent_entity.is_valid()) {
            auto parent_it = self.entity_transforms_map.find(parent_entity);
            if (parent_it != self.entity_transforms_map.end()) {
                auto transform_id = parent_it->second;
                auto *parent_gpu_transform = self.transforms.slot(transform_id);
                world_mat = parent_gpu_transform->world;
            }
        }

        auto cur_it = self.entity_transforms_map.find(cur_entity);
        if (cur_it == self.entity_transforms_map.end()) {
            continue;
        }

        auto transform_id = cur_it->second;
        auto *gpu_transform = self.transforms.slot(transform_id);
        gpu_transform->local = local_mat;
        gpu_transform->world = world_mat * local_mat;
        gpu_transform->normal = glm::mat3(gpu_transform->world);
        self.dirty_transforms.push_back(transform_id);

        cur_entity.children([&bfs_stack](flecs::entity e) {
            if (e.has<ECS::Transform>()) {
                bfs_stack.push(e);
            }
        });
    }
}

auto Scene::get_root(this Scene &self) -> flecs::entity {
    ZoneScoped;

    return self.root;
}

auto Scene::get_world(this Scene &self) -> flecs::world & {
    ZoneScoped;

    return self.world.value();
}

auto Scene::get_name(this Scene &self) -> const std::string & {
    ZoneScoped;

    return self.name;
}

auto Scene::get_name_sv(this Scene &self) -> std::string_view {
    ZoneScoped;

    return self.name;
}

auto Scene::get_cull_flags(this Scene &self) -> GPU::CullFlags & {
    return self.cull_flags;
}

auto Scene::prepare_frame(this Scene &self, SceneRenderer &renderer, u32 image_count, ls::option<GPU::Camera> override_camera) -> PreparedFrame {
    ZoneScoped;

    auto &asset_man = App::mod<AssetManager>();

    // clang-format off
    auto camera_query = self.get_world()
        .query_builder<ECS::Transform, ECS::Camera, ECS::ActiveCamera>()
        .build();
    auto directional_light_query = self.get_world()
        .query_builder<ECS::DirectionalLight>()
        .build();
    auto atmosphere_query = self.get_world()
        .query_builder<ECS::Atmosphere>()
        .build();
    auto eye_adaptation_query = self.get_world()
        .query_builder<ECS::EyeAdaptation>()
        .build();
    auto vbgtao_query = self.get_world()
        .query_builder<ECS::VBGTAO>()
        .build();
    auto rendering_meshes_query = self.get_world()
        .query_builder<ECS::RenderingMesh>()
        .build();
    // clang-format on

    ls::option<GPU::Camera> active_camera_data = override_camera;
    if (!active_camera_data.has_value()) {
        camera_query.each([&active_camera_data](flecs::entity, const ECS::Transform &t, const ECS::Camera &c, const ECS::ActiveCamera) {
            auto aspect_ratio = c.resolution.x / c.resolution.y;
            auto projection_mat = glm::perspectiveRH_ZO(glm::radians(c.fov), aspect_ratio, c.far_clip, c.near_clip);
            projection_mat[1][1] *= -1;

            auto direction = glm::vec3(
                glm::cos(glm::radians(t.rotation.x)) * glm::cos(glm::radians(t.rotation.y)),
                glm::sin(glm::radians(t.rotation.y)),
                glm::sin(glm::radians(t.rotation.x)) * glm::cos(glm::radians(t.rotation.y))
            );
            direction = glm::normalize(direction);
            auto view_mat = glm::lookAt(t.position, t.position + direction, glm::vec3(0.0f, 1.0f, 0.0f));

            auto &camera_data = active_camera_data.emplace(GPU::Camera{});
            camera_data.projection_mat = projection_mat;
            camera_data.inv_projection_mat = glm::inverse(projection_mat);
            camera_data.view_mat = view_mat;
            camera_data.inv_view_mat = glm::inverse(camera_data.view_mat);
            camera_data.projection_view_mat = camera_data.projection_mat * camera_data.view_mat;
            camera_data.inv_projection_view_mat = glm::inverse(camera_data.projection_view_mat);
            camera_data.position = t.position;
            camera_data.resolution = c.resolution;
            camera_data.near_clip = c.near_clip;
            camera_data.far_clip = c.far_clip;
            camera_data.acceptable_lod_error = c.acceptable_lod_error;
            camera_data.fov_deg = c.fov;
            camera_data.aspect_ratio = aspect_ratio;
        });
    }

    ls::option<GPU::DirectionalLight> directional_light_data = ls::nullopt;
    glm::mat4 directional_light_clipmap_projection_view_mats[GPU::DirectionalLight::MAX_CLIPMAP_COUNT] = {};
    directional_light_query.each([&directional_light_data,
                                  &directional_light_clipmap_projection_view_mats,
                                  &active_camera_data](flecs::entity, const ECS::DirectionalLight &directional_light_comp) {
        auto &directional_light = directional_light_data.emplace();

        auto light_dir_rad = glm::radians(directional_light_comp.direction);
        directional_light.direction = {
            glm::cos(light_dir_rad.x) * glm::sin(light_dir_rad.y),
            glm::sin(light_dir_rad.x) * glm::sin(light_dir_rad.y),
            glm::cos(light_dir_rad.y),
        };
        directional_light.base_ambient_color = directional_light_comp.base_ambient_color;
        directional_light.intensity = directional_light_comp.intensity;
        directional_light.clipmap_count = ls::min(directional_light_comp.clipmap_count, GPU::DirectionalLight::MAX_CLIPMAP_COUNT);
        directional_light.depth_bias = directional_light_comp.depth_bias;
        directional_light.normal_bias = directional_light_comp.normal_bias;

        if (directional_light_comp.clipmap_count > 0) {
            calculate_virtual_shadow_matrices(
                directional_light,
                directional_light_clipmap_projection_view_mats,
                directional_light_comp,
                *active_camera_data
            );
        }
    });

    ls::option<GPU::Atmosphere> atmosphere_data = ls::nullopt;
    atmosphere_query.each([&atmosphere_data, &active_camera_data, &directional_light_data](flecs::entity, const ECS::Atmosphere &atmos_comp) {
        auto &atmosphere = atmosphere_data.emplace();
        atmosphere.rayleigh_scatter = atmos_comp.rayleigh_scattering * 1e-3f;
        atmosphere.rayleigh_density = atmos_comp.rayleigh_density;
        atmosphere.mie_scatter = atmos_comp.mie_scattering * 1e-3f;
        atmosphere.mie_density = atmos_comp.mie_density;
        atmosphere.mie_extinction = atmos_comp.mie_extinction * 1e-3f;
        atmosphere.mie_asymmetry = atmos_comp.mie_asymmetry;
        atmosphere.ozone_absorption = atmos_comp.ozone_absorption * 1e-3f;
        atmosphere.ozone_height = atmos_comp.ozone_height;
        atmosphere.ozone_thickness = atmos_comp.ozone_thickness;
        atmosphere.terrain_albedo = atmos_comp.terrain_albedo;
        atmosphere.aerial_perspective_start_km = atmos_comp.aerial_perspective_start_km;

        if (active_camera_data.has_value()) {
            atmosphere.eye_height = active_camera_data->position.y * GPU::CAMERA_SCALE_UNIT;
            atmosphere.eye_height += atmosphere.planet_radius + GPU::PLANET_RADIUS_OFFSET;
        }

        if (directional_light_data.has_value()) {
            atmosphere.sun_direction = directional_light_data->direction;
            atmosphere.sun_intensity = directional_light_data->intensity;
        }
    });

    ls::option<GPU::EyeAdaptation> eye_adaptation_data = ls::nullopt;
    eye_adaptation_query.each([&eye_adaptation_data](flecs::entity, const ECS::EyeAdaptation &eye_adaptation_comp) {
        auto &eye_adaptation = eye_adaptation_data.emplace();
        eye_adaptation.min_exposure = eye_adaptation_comp.min_exposure;
        eye_adaptation.max_exposure = eye_adaptation_comp.max_exposure;
        eye_adaptation.adaptation_speed = eye_adaptation_comp.adaptation_speed;
        eye_adaptation.ISO_K = eye_adaptation_comp.ISO / eye_adaptation_comp.K;
    });

    auto regenerate_sky = false;
    if (atmosphere_data.has_value()) {
        auto &atmosphere = atmosphere_data.value();
        regenerate_sky |= self.last_atmosphere.rayleigh_scatter != atmosphere.rayleigh_scatter;
        regenerate_sky |= self.last_atmosphere.rayleigh_density != atmosphere.rayleigh_density;
        regenerate_sky |= self.last_atmosphere.mie_scatter != atmosphere.mie_scatter;
        regenerate_sky |= self.last_atmosphere.mie_density != atmosphere.mie_density;
        regenerate_sky |= self.last_atmosphere.mie_extinction != atmosphere.mie_extinction;
        regenerate_sky |= self.last_atmosphere.mie_asymmetry != atmosphere.mie_asymmetry;
        regenerate_sky |= self.last_atmosphere.ozone_absorption != atmosphere.ozone_absorption;
        regenerate_sky |= self.last_atmosphere.ozone_height != atmosphere.ozone_height;
        regenerate_sky |= self.last_atmosphere.ozone_thickness != atmosphere.ozone_thickness;
        regenerate_sky |= self.last_atmosphere.terrain_albedo != atmosphere.terrain_albedo;
        regenerate_sky |= self.last_atmosphere.atmos_radius != atmosphere.atmos_radius;
        regenerate_sky |= self.last_atmosphere.planet_radius != atmosphere.planet_radius;
        regenerate_sky |= self.last_atmosphere.terrain_albedo != atmosphere.terrain_albedo;
        self.last_atmosphere = atmosphere;
    }

    auto vbgtao = ls::option<GPU::VBGTAO>();
    vbgtao_query.each([&vbgtao](flecs::entity, ECS::VBGTAO &vbgtao_comp) {
        vbgtao.emplace(
            GPU::VBGTAO{
                .thickness = vbgtao_comp.thickness,
                .depth_range_scale_factor = vbgtao_comp.depth_range_scale_factor,
                .radius = vbgtao_comp.radius,
                .radius_multiplier = vbgtao_comp.radius_multiplier,
                .slice_count = glm::ceil(vbgtao_comp.slice_count),
                .sample_count_per_slice = glm::ceil(vbgtao_comp.sample_count_per_slice),
                .denoise_power = vbgtao_comp.denoise_power,
                .linear_thickness_multiplier = vbgtao_comp.linear_thickness_multiplier,
            }
        );
    });

    auto meshlet_instance_visibility_offset = 0_u32;
    auto max_meshlet_instance_count = 0_u32;
    auto gpu_meshes = std::vector<GPU::Mesh>();
    auto gpu_mesh_instances = std::vector<GPU::MeshInstance>();

    if (self.models_dirty) {
        for (const auto &[rendering_mesh, transform_ids] : self.rendering_meshes_map) {
            auto *model = asset_man.get_model(rendering_mesh.n0);
            const auto &mesh = model->meshes[rendering_mesh.n1];

            for (auto primitive_index : mesh.primitive_indices) {
                const auto &primitive = model->primitives[primitive_index];
                const auto &gpu_mesh = model->gpu_meshes[primitive_index];
                auto mesh_index = static_cast<u32>(gpu_meshes.size());
                gpu_meshes.emplace_back(gpu_mesh);

                //  ── INSTANCING ──────────────────────────────────────────────────
                for (const auto transform_id : transform_ids) {
                    auto lod0_index = 0;
                    const auto &lod0 = gpu_mesh.lods[lod0_index];

                    auto &mesh_instance = gpu_mesh_instances.emplace_back();
                    mesh_instance.mesh_index = mesh_index;
                    mesh_instance.lod_index = lod0_index;
                    mesh_instance.material_index = SlotMap_decode_id(primitive.material_id).index;
                    mesh_instance.transform_index = SlotMap_decode_id(transform_id).index;
                    mesh_instance.meshlet_instance_visibility_offset = meshlet_instance_visibility_offset;

                    meshlet_instance_visibility_offset += lod0.meshlet_count;
                    max_meshlet_instance_count += lod0.meshlet_count;
                }
            }
        }

        self.mesh_instance_count = gpu_mesh_instances.size();
        self.max_meshlet_instance_count = max_meshlet_instance_count;
    }

    auto uuid_to_image_index = [&](const UUID &uuid) -> ls::option<u32> {
        if (!asset_man.is_texture_loaded(uuid)) {
            return ls::nullopt;
        }

        auto *texture = asset_man.get_texture(uuid);
        return texture->image_view.index();
    };

    auto dirty_material_ids = asset_man.get_dirty_material_ids();
    auto dirty_material_indices = std::vector<u32>();
    for (const auto dirty_id : dirty_material_ids) {
        const auto *material = asset_man.get_material(dirty_id);
        if (!material) {
            continue;
        }

        auto dirty_index = SlotMap_decode_id(dirty_id).index;
        dirty_material_indices.push_back(dirty_index);
        if (dirty_index >= self.gpu_materials.size()) {
            self.gpu_materials.resize(dirty_index + 1, {});
        }

        auto albedo_image_index = uuid_to_image_index(material->albedo_texture);
        auto normal_image_index = uuid_to_image_index(material->normal_texture);
        auto emissive_image_index = uuid_to_image_index(material->emissive_texture);
        auto metallic_roughness_image_index = uuid_to_image_index(material->metallic_roughness_texture);
        auto occlusion_image_index = uuid_to_image_index(material->occlusion_texture);
        auto sampler_index = 0_u32;

        auto flags = GPU::MaterialFlag::None;
        if (albedo_image_index.has_value()) {
            auto *texture = asset_man.get_texture(material->albedo_texture);
            sampler_index = texture->sampler.index();
            flags |= GPU::MaterialFlag::HasAlbedoImage;
        }

        flags |= normal_image_index.has_value() ? GPU::MaterialFlag::HasNormalImage : GPU::MaterialFlag::None;
        flags |= emissive_image_index.has_value() ? GPU::MaterialFlag::HasEmissiveImage : GPU::MaterialFlag::None;
        flags |= metallic_roughness_image_index.has_value() ? GPU::MaterialFlag::HasMetallicRoughnessImage : GPU::MaterialFlag::None;
        flags |= occlusion_image_index.has_value() ? GPU::MaterialFlag::HasOcclusionImage : GPU::MaterialFlag::None;

        auto gpu_material = GPU::Material{
            .albedo_color = material->albedo_color,
            .emissive_color = material->emissive_color,
            .roughness_factor = material->roughness_factor,
            .metallic_factor = material->metallic_factor,
            .alpha_cutoff = material->alpha_cutoff,
            .flags = flags,
            .sampler_index = sampler_index,
            .albedo_image_index = albedo_image_index.value_or(0_u32),
            .normal_image_index = normal_image_index.value_or(0_u32),
            .emissive_image_index = emissive_image_index.value_or(0_u32),
            .metallic_roughness_image_index = metallic_roughness_image_index.value_or(0_u32),
            .occlusion_image_index = occlusion_image_index.value_or(0_u32),
        };

        self.gpu_materials[dirty_index] = gpu_material;
    }

    auto prepare_info = FramePrepareInfo{
        .image_count = image_count,
        .mesh_instance_count = self.mesh_instance_count,
        .max_meshlet_instance_count = self.max_meshlet_instance_count,
        .regenerate_sky = regenerate_sky,
        .dirty_transform_ids = self.dirty_transforms,
        .gpu_transforms = self.transforms.slots_unsafe(),
        .dirty_material_indices = dirty_material_indices,
        .gpu_materials = self.gpu_materials,
        .gpu_meshes = gpu_meshes,
        .gpu_mesh_instances = gpu_mesh_instances,
        .camera = active_camera_data.value_or(GPU::Camera{}),
        .directional_light = directional_light_data,
        .directional_light_clipmap_projection_view_mats = directional_light_clipmap_projection_view_mats,
        .atmosphere = atmosphere_data,
        .eye_adaptation = eye_adaptation_data,
        .vbgtao = vbgtao,
    };
    auto prepared_frame = renderer.prepare_frame(prepare_info);

    self.models_dirty = false;
    self.dirty_transforms.clear();

    return prepared_frame;
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

auto Scene::attach_mesh(this Scene &self, flecs::entity entity, const UUID &model_uuid, usize mesh_index) -> bool {
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
    for (const auto &[cur_old_rendering_mesh, transform_ids] : self.rendering_meshes_map) {
        if (std::ranges::find(transform_ids, transform_id) != transform_ids.end()) {
            old_model_uuid = cur_old_rendering_mesh.n0;
            break;
        }
    }
    if (old_model_uuid) {
        self.detach_mesh(entity, old_model_uuid, mesh_index);
    }

    auto instances_it = self.rendering_meshes_map.find(ls::pair(model_uuid, mesh_index));
    if (instances_it == self.rendering_meshes_map.end()) {
        bool inserted = false;
        std::tie(instances_it, inserted) = self.rendering_meshes_map.try_emplace(ls::pair(model_uuid, mesh_index));
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

auto Scene::detach_mesh(this Scene &self, flecs::entity entity, const UUID &model_uuid, usize mesh_index) -> bool {
    ZoneScoped;

    auto instances_it = self.rendering_meshes_map.find(ls::pair(model_uuid, mesh_index));
    if (instances_it == self.rendering_meshes_map.end()) {
        return false;
    }

    auto should_remove = true;
    auto transforms_it = self.entity_transforms_map.find(entity);
    if (transforms_it != self.entity_transforms_map.end()) {
        const auto transform_id = transforms_it->second;
        auto &instances = instances_it->second;
        std::erase_if(instances, [transform_id](const GPU::TransformID &id) { return id == transform_id; });

        should_remove = not instances.empty();
    }

    if (should_remove) {
        self.rendering_meshes_map.erase(instances_it);
    }

    self.models_dirty = true;

    return true;
}

} // namespace lr
