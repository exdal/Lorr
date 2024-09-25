#include "World.hh"

#include "Components.hh"

#include "Engine/Memory/Stack.hh"
#include "Engine/OS/OS.hh"
#include "Engine/Util/json_driver.hh"

#include <glaze/glaze.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace lr {
bool World::init(this World &self) {
    ZoneScoped;

    // SETUP CUSTOM TYPES
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

    // SETUP REFLECTION
    Component::reflect_all(self.ecs);

    // SETUP SYSTEMS
    self.ecs.system<Component::PerspectiveCamera, Component::Transform, Component::Camera>()
        .kind(flecs::OnUpdate)
        .each([](flecs::iter &it, usize, Component::PerspectiveCamera, Component::Transform &t, Component::Camera &c) {
            t.rotation.x = std::fmod(t.rotation.x, 360.0f);
            t.rotation.y = glm::clamp(t.rotation.y, -89.0f, 89.0f);

            auto inv_orient = glm::conjugate(c.orientation);
            t.position += glm::vec3(inv_orient * glm::vec4(c.velocity * it.delta_time(), 0.0f));

            c.projection = glm::perspectiveLH(glm::radians(c.fov), c.aspect_ratio, 0.01f, 10000.0f);
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

    return true;
}

void World::shutdown(this World &self) {
    ZoneScoped;

    self.ecs.quit();
}

bool World::poll(this World &self) {
    ZoneScoped;

    return self.ecs.progress();
}

bool World::import_scene(this World &self, Scene &scene, const fs::path &path) {
    ZoneScoped;

    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    auto json_str = file.read_string({ 0, ~0_sz });
    generic_json json = {};
    auto json_err = glz::read_json(json, json_str);
    if (json_err != glz::error_code::none) {
        LOG_ERROR("Failed to open scene file! {}", json_err.custom_error_message);
        return false;
    }

    auto &name_json = json.at("name");
    if (!name_json) {
        LOG_ERROR("Scene files must have names!");
        return false;
    }

    LS_EXPECT(name_json.is_string());
    scene.handle.set_name(name_json.get_string().c_str());

    auto &entities_json = json.at("entities");
    for (auto &entity_json : entities_json.get_array()) {
        auto &entity_name_json = entity_json.at("name");
        if (!entity_name_json) {
            LOG_ERROR("Entities must have names!");
            return false;
        }

        LS_EXPECT(entity_name_json.is_string());
        auto e = scene.create_entity(entity_name_json.get_string());

        auto &entity_tags_json = entity_json.at("tags");
        LS_EXPECT(entity_tags_json.is_array());
        for (auto &entity_tag : entity_tags_json.get_array()) {
            LS_EXPECT(entity_tag.is_string());

            auto tag = self.ecs.component(entity_tag.get_string().c_str());
            e.add(tag);
        }

        auto &components_json = entity_json.at("components");
        LS_EXPECT(components_json.is_array());
        for (auto &component_json : components_json.get_array()) {
            LS_EXPECT(component_json.is_object());

            auto &component_name_json = component_json.at("name");
            LS_EXPECT(component_name_json.is_string());
            if (!component_name_json) {
                LOG_ERROR("Entity '{}' has corrupt components JSON array.", e.name());
                return false;
            }

            auto &component_name = component_name_json.get_string();
            auto component_id = self.ecs.lookup(component_name.c_str());
            if (!component_id) {
                LOG_ERROR("Entity '{}' has invalid component named '{}'!", e.name(), component_name);
                return false;
            }

            e.add(component_id);
            Component::Wrapper component(e, component_id);
            component.for_each([&](usize &, std::string_view member_name, Component::Wrapper::Member &member) {
                auto &member_json = component_json.at(member_name);
                std::visit(
                    match{
                        [](const auto &) {},
                        [&](f32 *v) {
                            LS_EXPECT(member_json.is_float());
                            *v = member_json.as<f32>();
                        },
                        [&](i32 *v) {
                            LS_EXPECT(member_json.is_integer());
                            *v = member_json.as<i32>();
                        },
                        [&](u32 *v) {
                            LS_EXPECT(member_json.is_integer());
                            *v = member_json.as<u32>();
                        },
                        [&](i64 *v) {
                            LS_EXPECT(member_json.is_integer());
                            *v = member_json.as<i64>();
                        },
                        [&](u64 *v) {
                            LS_EXPECT(member_json.is_integer());
                            *v = member_json.as<u64>();
                        },
                        [&](glm::vec2 *v) {
                            LS_EXPECT(member_json.is<glm::vec2>());
                            *v = member_json.as<glm::vec2>();
                        },
                        [&](glm::vec3 *v) {
                            LS_EXPECT(member_json.is<glm::vec3>());
                            *v = member_json.as<glm::vec3>();
                        },
                        [&](glm::vec4 *v) {
                            LS_EXPECT(member_json.is<glm::vec4>());
                            *v = member_json.as<glm::vec4>();
                        },
                        [&](std::string *v) {
                            LS_EXPECT(member_json.is_string());
                            *v = member_json.as<std::string>();
                        },
                    },
                    member);
            });
        }
    }

    return true;
}

bool World::export_scene(this World &, Scene &scene, const fs::path &dir) {
    ZoneScoped;

    generic_json json = {};
    json["name"] = std::string(scene.name());
    auto entities_json = generic_json::array_t{};
    scene.children([&](flecs::entity e) {
        auto &entity_json = entities_json.emplace_back();
        entity_json["name"] = std::string(e.name(), e.name().length());

        generic_json::array_t entity_tags_json = {};
        generic_json::array_t entity_components_json = {};

        e.each([&](flecs::id component_id) {
            auto world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            Component::Wrapper component(e, component_id);
            if (!component.has_component()) {
                // It's a tag
                entity_tags_json.emplace_back(std::string(component.name));
                return;
            }

            auto &entity_component_json = entity_components_json.emplace_back();
            entity_component_json["name"] = std::string(component.name);
            component.for_each([&](usize &, std::string_view member_name, Component::Wrapper::Member &member) {
                generic_json member_json = {};
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

                entity_component_json[member_name] = member_json;
            });
        });

        entity_json["tags"] = entity_tags_json;
        entity_json["components"] = entity_components_json;
    });

    json["entities"] = entities_json;

    std::string json_str;
    auto json_str_err = glz::write<glz::opts{ .prettify = true, .indentation_width = 2 }>(json, json_str);
    if (json_str_err != glz::error_code::none) {
        LOG_ERROR("Failed to serialze scene '{}'! {}", scene.name(), json_str_err.custom_error_message);
        return false;
    }

    auto clear_name = std::format("{}.lrscene", scene.name());
    auto file_path = dir / clear_name;
    File file(file_path, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", file_path);
        return false;
    }

    file.write(json_str.data(), { 0, json_str.length() });

    return true;
}

bool World::import_project(this World &self, const fs::path &path) {
    ZoneScoped;

    File project_file(path, FileAccess::Read);
    if (!project_file) {
        LOG_ERROR("Failed to read '{}'!", path);
        return false;
    }

    ProjectInfo project_info = {};
    auto json = project_file.read_string({ 0, project_file.size });

    if (auto err = glz::read_json(project_info, json)) {
        LOG_ERROR("Failed to read project! {}", err.custom_error_message);
        return false;
    }

    const fs::path &dir = path.parent_path();

    self.project_info = project_info;
    self.name = project_info.project_name;
    fs::path models_path = dir / project_info.models_path;
    fs::path scenes_path = dir / project_info.scenes_path;

    return true;
}

bool World::export_project(this World &self, const fs::path &root_dir) {
    ZoneScoped;

    // root_dir = /path/to/my_projects
    // proj_root_dir = root_dir / <PROJECT_NAME>
    // proj_file = proj_root_dir / world.lrproj
    auto proj_root_dir = root_dir / self.name;
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
            .project_name = self.name,
            .scenes_path = dir_tree[2],
            .models_path = dir_tree[3],
        };
        self.project_info = new_proj_info;
    }

    LS_EXPECT(self.project_info.has_value());

    for (auto &scene : self.scenes) {
        self.export_scene(*scene, proj_root_dir / self.project_info->scenes_path);
    }

    std::string json_str;
    auto json_str_err = glz::write<glz::opts{ .prettify = true, .indentation_width = 2 }>(self.project_info.value(), json_str);
    if (json_str_err != glz::error_code::none) {
        LOG_ERROR("Failed to write for file '{}'! {}", proj_file, json_str_err.custom_error_message);
        return false;
    }

    File file(proj_file, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", proj_file);
        return false;
    }

    file.write(json_str.data(), { 0, json_str.length() });

    return true;
}

void World::set_active_scene(this World &self, SceneID scene_id) {
    ZoneScoped;

    usize scene_idx = static_cast<usize>(scene_id);
    if (scene_id != SceneID::Invalid && scene_idx < self.scenes.size()) {
        self.active_scene = scene_id;
    }
}

}  // namespace lr
