#include "World.hh"

#include "Components.hh"

#include "Engine/Core/Application.hh"
#include "Engine/Memory/Stack.hh"
#include "Engine/OS/OS.hh"

#include <glm/gtx/euler_angles.hpp>
#include <yyjson.h>

namespace lr {
bool World::init(this World &self) {
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

// Holy fuck, i hate these functions

bool World::import_scene(this World &self, SceneID scene_id, const fs::path &path) {
    ZoneScoped;

    auto &scene = self.scene_at(scene_id);
    File file(path, FileAccess::Read);
    if (!file) {
        LR_LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    auto json_str = file.read_string({ 0, ~0_sz });
    yyjson_read_err doc_err;
    auto *doc = yyjson_read_opts(json_str.data(), json_str.length(), 0, nullptr, &doc_err);
    LS_DEFER(&) {
        yyjson_doc_free(doc);
    };

    auto *root = yyjson_doc_get_root(doc);
    auto *name_obj = yyjson_obj_get(root, "name");
    if (!name_obj) {
        LR_LOG_ERROR("Scene files must have names!");
        return false;
    }

    scene.handle.set_name(yyjson_get_str(name_obj));

    auto *entities_obj = yyjson_obj_get(root, "entities");
    yyjson_val *entity_obj = nullptr;
    usize elem_idx, arr_count;
    yyjson_arr_foreach(entities_obj, elem_idx, arr_count, entity_obj) {
        auto *entity_name_obj = yyjson_obj_get(entity_obj, "name");
        if (!entity_name_obj) {
            LR_LOG_ERROR("Entities must have names!");
            return false;
        }

        auto e = scene.create_entity(yyjson_get_str(entity_name_obj));

        auto *tags_obj = yyjson_obj_get(entity_obj, "tags");
        usize tag_idx, tag_count;
        yyjson_val *tag_obj = nullptr;
        yyjson_arr_foreach(tags_obj, tag_idx, tag_count, tag_obj) {
            auto *tag_name = yyjson_get_str(tag_obj);
            auto tag = self.ecs.component(tag_name);
            e.add(tag);
        }

        auto *components_obj = yyjson_obj_get(entity_obj, "components");
        usize component_idx, component_count;
        yyjson_val *component_obj;
        yyjson_arr_foreach(components_obj, component_idx, component_count, component_obj) {
            auto *component_name_obj = yyjson_obj_get(component_obj, "name");
            if (!component_name_obj) {
                LR_LOG_ERROR("Components must have names!");
                return false;
            }

            auto *component_name = yyjson_get_str(component_name_obj);
            auto component_id = self.ecs.lookup(component_name);
            if (!component_id) {
                LR_LOG_ERROR("Invalid component '{}'!", component_name);
                return false;
            }

            e.add(component_id);

            auto *component_data = component_id.get<flecs::Struct>();
            auto member_count = ecs_vec_count(&component_data->members);
            auto *members = static_cast<ecs_member_t *>(ecs_vec_first(&component_data->members));
            auto *ptr = static_cast<u8 *>(e.get_mut(component_id));
            for (i32 i = 0; i < member_count; i++) {
                const auto &member = members[i];
                auto member_type = flecs::entity(self.ecs, member.type);
                auto member_offset = member.offset;

                auto *member_obj = yyjson_obj_get(component_obj, member.name);

                if (member_type == flecs::F32) {
                    auto &v = *reinterpret_cast<f32 *>(ptr + member_offset);

                    v = static_cast<f32>(yyjson_get_real(member_obj));
                } else if (member_type == flecs::I32 || member_type == flecs::U32) {
                    auto &v = *reinterpret_cast<i32 *>(ptr + member_offset);

                    v = yyjson_get_int(member_obj);
                } else if (member_type == flecs::I64 || member_type == flecs::U64) {
                    auto &v = *reinterpret_cast<i64 *>(ptr + member_offset);

                    v = yyjson_get_sint(member_obj);

                } else if (
                    member_type == self.ecs.entity<glm::vec2>()     //
                    || member_type == self.ecs.entity<glm::vec3>()  //
                    || member_type == self.ecs.entity<glm::vec4>()) {
                    auto v = reinterpret_cast<f32 *>(ptr + member_offset);

                    usize i, _;
                    yyjson_val *m = nullptr;
                    yyjson_arr_foreach(member_obj, i, _, m) {
                        v[i] = static_cast<f32>(yyjson_get_real(m));
                    }
                } else if (member_type == self.ecs.entity<std::string>()) {
                    auto &v = *reinterpret_cast<std::string *>(ptr + member_offset);

                    auto *str = yyjson_get_str(member_obj);
                    auto str_len = yyjson_get_len(member_obj);
                    v = std::string(str, str_len);
                }
            }
        }
    }

    return true;
}

bool World::export_scene(this World &self, SceneID scene_id, const fs::path &path) {
    ZoneScoped;

    auto &scene = self.scene_at(scene_id);
    const auto &scene_name = scene.handle.name();
    auto *doc = yyjson_mut_doc_new(nullptr);
    auto *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_strn(doc, root, "name", scene_name, scene_name.length());

    auto *entities_obj = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "entities", entities_obj);

    scene.children([&](flecs::entity e) {
        auto *entity_obj = yyjson_mut_obj(doc);
        yyjson_mut_arr_append(entities_obj, entity_obj);
        yyjson_mut_obj_add_strn(doc, entity_obj, "name", e.name(), e.name().length());

        auto *tags_obj = yyjson_mut_arr(doc);
        yyjson_mut_obj_add_val(doc, entity_obj, "tags", tags_obj);

        auto *components_obj = yyjson_mut_arr(doc);
        yyjson_mut_obj_add_val(doc, entity_obj, "components", components_obj);

        e.each([&](flecs::id component_id) {
            auto world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            auto component_entity = component_id.entity();
            auto component_name = component_entity.symbol();

            if (!component_entity.has<flecs::Struct>()) {
                // It's a tag
                yyjson_mut_arr_add_strn(doc, tags_obj, component_name, component_name.length());

                return;
            }

            auto *component_obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_strn(doc, component_obj, "name", component_name, component_name.length());

            auto component_data = component_entity.get<flecs::Struct>();
            i32 member_count = ecs_vec_count(&component_data->members);
            auto members = static_cast<ecs_member_t *>(ecs_vec_first(&component_data->members));
            auto ptr = static_cast<u8 *>(e.get_mut(component_id));
            for (i32 i = 0; i < member_count; i++) {
                const auto &member = members[i];
                auto member_type = flecs::entity(world, member.type);
                auto member_offset = member.offset;

                if (member_type == flecs::F32) {
                    auto v = *reinterpret_cast<f32 *>(ptr + member_offset);

                    yyjson_mut_obj_add_real(doc, component_obj, member.name, v);
                } else if (member_type == flecs::I32 || member_type == flecs::U32) {
                    auto v = *reinterpret_cast<i32 *>(ptr + member_offset);

                    yyjson_mut_obj_add_int(doc, component_obj, member.name, v);
                } else if (member_type == flecs::I64 || member_type == flecs::U64) {
                    auto v = *reinterpret_cast<i64 *>(ptr + member_offset);

                    yyjson_mut_obj_add_sint(doc, component_obj, member.name, v);
                } else if (member_type == world.entity<glm::vec2>()) {
                    auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                    auto *vec_obj = yyjson_mut_arr_with_float(doc, v, 2);

                    yyjson_mut_obj_add_val(doc, component_obj, member.name, vec_obj);
                } else if (member_type == world.entity<glm::vec3>()) {
                    auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                    auto *vec_obj = yyjson_mut_arr_with_float(doc, v, 3);

                    yyjson_mut_obj_add_val(doc, component_obj, member.name, vec_obj);
                } else if (member_type == world.entity<glm::vec4>()) {
                    auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                    auto *vec_obj = yyjson_mut_arr_with_float(doc, v, 4);

                    yyjson_mut_obj_add_val(doc, component_obj, member.name, vec_obj);
                } else if (member_type == world.entity<std::string>()) {
                    auto v = reinterpret_cast<std::string *>(ptr + member_offset);

                    yyjson_mut_obj_add_strn(doc, component_obj, member.name, v->c_str(), v->length());
                }
            }

            yyjson_mut_arr_add_val(components_obj, component_obj);
        });
    });

    usize json_len = 0;
    auto *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END | YYJSON_WRITE_ESCAPE_UNICODE, &json_len);
    LS_DEFER(&) {
        if (json) {
            free(json);
        }

        yyjson_mut_doc_free(doc);
    };

    File file(path, FileAccess::Write);
    if (!file) {
        LR_LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    file.write(json, { 0, json_len });

    return true;
}

bool World::export_project(this World &self, const fs::path &path) {
    ZoneScoped;
    memory::ScopedStack stack;

    // Project Name
    // - Audio
    // - Models
    // - Scenes
    // - Shaders
    // - Prefabs
    // world.lrproj

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    auto *doc = yyjson_mut_doc_new(nullptr);
    auto *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_strn(doc, root, "project_name", self.name.c_str(), self.name.length());

    auto *models_obj = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "models", models_obj);
    for (auto &path : asset_man.model_paths) {
        auto path_str = path.string();
        auto path_sv = stack.to_utf8(path.u16string());
        yyjson_mut_arr_add_strn(doc, models_obj, path_sv.data(), path_sv.length());
    }

    yyjson_write_err err;
    usize json_len = 0;
    auto *json = yyjson_mut_write_opts(doc, YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END | YYJSON_WRITE_ESCAPE_UNICODE, nullptr, &json_len, &err);
    LS_DEFER(&) {
        if (json) {
            free(json);
        }

        yyjson_mut_doc_free(doc);
    };

    File file(path, FileAccess::Write);
    if (!file) {
        LR_LOG_ERROR("Failed to open file {}!", path);
        return false;
    }

    file.write(json, { 0, json_len });

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
