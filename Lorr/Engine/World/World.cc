#include "World.hh"

#include "Engine/Core/Application.hh"
#include "Engine/OS/OS.hh"
#include "Engine/Util/JsonWriter.hh"
#include "Engine/World/Components.hh"
#include "Engine/World/RuntimeScene.hh"

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

bool World::init(this World &self) {
    ZoneScoped;

    self.renderer = WorldRenderer::create(Application::get().device);
    self.unload_active_project();

    return true;
}

void World::setup_ecs(this World &self) {
    ZoneScoped;

    self.ecs
        .component<glm::vec2>("glm::vec2")  //
        .member<f32>("x")
        .member<f32>("y");

    self.ecs
        .component<glm::vec3>("glm::vec3")  //
        .member<f32>("x")
        .member<f32>("y")
        .member<f32>("z");

    self.ecs
        .component<glm::vec4>("glm::vec4")  //
        .member<f32>("x")
        .member<f32>("y")
        .member<f32>("z")
        .member<f32>("w");

    self.ecs
        .component<glm::mat3>("glm::mat3")  //
        .member<glm::vec3>("col0")
        .member<glm::vec3>("col1")
        .member<glm::vec3>("col2");

    self.ecs
        .component<glm::mat4>("glm::mat4")  //
        .member<glm::vec4>("col0")
        .member<glm::vec4>("col1")
        .member<glm::vec4>("col2")
        .member<glm::vec4>("col3");

    self.ecs.component<std::string>("std::string")
        .opaque(flecs::String)
        .serialize([](const flecs::serializer *s, const std::string *data) {
            const char *str = data->c_str();
            return s->value(flecs::String, &str);
        })
        .assign_string([](std::string *data, const char *value) { *data = value; });

    self.ecs.component<Identifier>("Identifier")
        .opaque(flecs::String)
        .serialize([](const flecs::serializer *s, const Identifier *data) {
            const char *str = data->sv().data();
            return s->value(flecs::String, &str);
        })
        .assign_string([](Identifier *data, const char *value) { *data = Identifier(value); });

    // SETUP REFLECTION
    Component::reflect_all(self.ecs);

    // SETUP SYSTEMS
    self.ecs.system<Component::PerspectiveCamera, Component::Transform, Component::Camera>()
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

    self.ecs.system<Component::RenderableModel, Component::Transform, Component::RenderableModel>()
        .kind(flecs::OnUpdate)
        .each([](flecs::iter &, usize, Component::RenderableModel, Component::Transform &t, Component::RenderableModel &) {
            auto rotation = glm::radians(t.rotation);

            t.matrix = glm::translate(glm::mat4(1.0), t.position);
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
            t.matrix *= glm::scale(glm::mat4(1.0), t.scale);
        });
}

void World::shutdown(this World &self) {
    ZoneScoped;

    self.ecs.quit();
}

void World::begin_frame(this World &self) {
    ZoneScoped;

    if (!self.active_scene.has_value()) {
        return;
    }

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &scene = self.scene_at(self.active_scene.value());

    auto camera_query =  //
        self.ecs
            .query_builder<Component::Transform, Component::Camera>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();
    auto model_transform_query =  //
        self.ecs
            .query_builder<Component::Transform, Component::RenderableModel>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();

    auto scene_begin_info = self.renderer.begin_scene_render_data(camera_query.count(), model_transform_query.count());
    if (!scene_begin_info.has_value()) {
        return;
    }

    auto camera_ptr = reinterpret_cast<GPUCameraData *>(scene_begin_info->cameras_allocation.ptr);
    camera_query.each([&](Component::Transform &t, Component::Camera &c) {
        camera_ptr->projection_mat = c.projection;
        camera_ptr->view_mat = t.matrix;
        camera_ptr->projection_view_mat = glm::transpose(c.projection * t.matrix);
        camera_ptr->inv_view_mat = glm::inverse(glm::transpose(t.matrix));
        camera_ptr->inv_projection_view_mat = glm::inverse(glm::transpose(c.projection)) * camera_ptr->inv_view_mat;
        camera_ptr->position = t.position;
        camera_ptr->near_clip = c.near_clip;
        camera_ptr->far_clip = c.far_clip;

        camera_ptr++;
    });

    auto model_transforms_ptr = reinterpret_cast<GPUModelTransformData *>(scene_begin_info->model_transfors_allocation.ptr);
    model_transform_query.each([&](Component::Transform &t, Component::RenderableModel &) {
        auto rotation = glm::radians(t.rotation);

        glm::mat4 &world_mat = model_transforms_ptr->world_transform_mat;
        world_mat = glm::translate(glm::mat4(1.0), glm::vec3(0.0f));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
        world_mat *= glm::scale(glm::mat4(1.0), t.scale);

        model_transforms_ptr->model_transform_mat = t.matrix;
        model_transforms_ptr++;
    });

    scene_begin_info->materials_buffer_id = asset_man.material_buffer().id();
    scene_begin_info->gpu_models = asset_man.gpu_models();
    self.renderer.end_scene_render_data(scene_begin_info.value());

    self.ecs.progress();
}

void World::end_frame(this World &) {
    ZoneScoped;
}

bool World::import_scene(this World &self, Scene &scene, const fs::path &path) {
    ZoneScoped;
    memory::ScopedStack stack;

    namespace sj = simdjson;

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), { 0, file.size });

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

    scene.handle.set_name(stack.null_terminate(name_json.value()).data());

    auto entities_json = doc["entities"].get_array();
    for (auto entity_json : entities_json) {
        auto entity_name_json = entity_json["name"];
        if (entity_name_json.error()) {
            LOG_ERROR("Entities must have names!");
            return false;
        }

        auto e = scene.create_entity(stack.null_terminate(entity_name_json.get_string()));

        auto entity_tags_json = entity_json["tags"];
        for (auto entity_tag : entity_tags_json.get_array()) {
            auto tag = self.ecs.component(stack.null_terminate(entity_tag.get_string()).data());
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
            auto component_id = self.ecs.lookup(component_name.data());
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
                    },
                    member);
            });
        }
    }

    return true;
}

bool World::export_scene(this World &, Scene &scene, const fs::path &dir) {
    ZoneScoped;

    JsonWriter json;
    json.begin_obj();
    json["name"] = scene.name();
    json["entities"].begin_array();
    scene.children([&](flecs::entity e) {
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

    auto clear_name = std::format("{}.lrscene", scene.name());
    auto file_path = dir / clear_name;
    File file(file_path, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", file_path);
        return false;
    }

    file.write(json.stream.view().data(), { 0, json.stream.view().length() });

    return true;
}

bool World::import_project(this World &self, const fs::path &path) {
    ZoneScoped;

    namespace sj = simdjson;

    const fs::path &project_root = path.parent_path();
    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to read '{}'!", path);
        return false;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), { 0, file.size });

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse scene file! {}", sj::error_message(doc.error()));
        return false;
    }

    ProjectInfo project_info = {};
    auto name_json = doc["name"];
    if (name_json.error() || !name_json.is_string()) {
        LOG_ERROR("Project file must have `name` value as string.");
        return false;
    }

    project_info.project_name = std::string(name_json.get_string().value());
    project_info.project_root_path = project_root;

    auto scenes_json = doc["scenes"];
    if (scenes_json.error() || !scenes_json.is_string()) {
        LOG_ERROR("Project file must have `scenes` value as string.");
        return false;
    }
    project_info.scenes_path = project_root / fs::path(scenes_json.get_string().value());

    auto models_json = doc["models"];
    if (models_json.error() || !models_json.is_string()) {
        LOG_ERROR("Project file must have `models` value as string.");
        return false;
    }
    project_info.models_path = project_root / fs::path(models_json.get_string().value());

    self.unload_active_project();
    self.project_info = project_info;

    for (const auto &iter : fs::directory_iterator(project_info.scenes_path)) {
        if (fs::is_directory(iter)) {
            continue;
        }

        const auto &scene_path = iter.path();
        self.create_scene_from_file<RuntimeScene>(scene_path);
    }

    self.set_active_scene(static_cast<SceneID>(0));

    return true;
}

bool World::export_project(this World &self, std::string_view project_name, const fs::path &root_dir) {
    ZoneScoped;

    // root_dir = /path/to/my_projects
    // proj_root_dir = root_dir / <PROJECT_NAME>
    // proj_file = proj_root_dir / world.lrproj
    auto proj_root_dir = root_dir / project_name;
    auto proj_file = proj_root_dir / "world.lrproj";

    // Project Name
    // - Audio
    // - Models
    // - Scenes
    // - Shaders
    // - Prefabs
    // world.lrproj

    // Fresh project, the dev has freedom to change file structure
    // but we prepare initial structure to ease out some stuff
    if (!fs::exists(proj_root_dir)) {
        // /<root_dir>/<name>/[dir_tree*]
        // clang-format off
        constexpr static std::string_view dir_tree[] = {
            "Assets",
            "Assets/Audio",
            "Assets/Scenes",
            "Assets/Shaders",
            "Assets/Models",
        };
        // clang-format on

        // Create project root directory first
        std::error_code err;
        fs::create_directories(proj_root_dir, err);
        if (err) {
            LOG_ERROR("Failed to create directory '{}'! {}", proj_root_dir, err.message());
            return false;
        }

        for (const auto &dir : dir_tree) {
            fs::create_directories(proj_root_dir / dir);
        }

        ProjectInfo new_proj_info = {
            .project_name = std::string(project_name),
            .project_root_path = proj_root_dir,
            .scenes_path = dir_tree[2],
            .models_path = dir_tree[3],
        };
        self.project_info = new_proj_info;
    }

    LS_EXPECT(self.project_info.has_value());

    for (auto &scene : self.scenes) {
        self.export_scene(*scene, proj_root_dir / self.project_info->scenes_path);
    }

    JsonWriter json;
    json.begin_obj();
    json["name"] = self.project_info->project_name;
    json["scenes"] = self.project_info->scenes_path;
    json["models"] = self.project_info->models_path;
    json.end_obj();

    File file(proj_file, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", proj_file);
        return false;
    }

    auto json_str = json.stream.view();
    file.write(json_str.data(), { 0, json_str.length() });

    return true;
}

bool World::unload_active_project(this World &self) {
    ZoneScoped;

    self.ecs.reset();
    self.active_scene.reset();
    self.scenes.clear();
    self.project_info.reset();

    self.setup_ecs();
    self.root = self.ecs.entity();

    return true;
}

bool World::save_active_project(this World &self) {
    ZoneScoped;

    LS_EXPECT(self.is_project_active());
    return self.export_project(self.project_info->project_name, self.project_info->project_root_path.parent_path());
}

void World::set_active_scene(this World &self, SceneID scene_id) {
    ZoneScoped;

    usize scene_idx = static_cast<usize>(scene_id);
    if (scene_id != SceneID::Invalid && scene_idx < self.scenes.size()) {
        self.active_scene = scene_id;
    }
}

}  // namespace lr
