#pragma once

#include "Engine/World/Components.hh"

namespace lr::Module {
struct CoreECS {
    CoreECS(flecs::world &world_) {
        world_.module<CoreECS>("lr::Core");

        // SETUP CUSTOM TYPES
        world_
            .component<glm::vec2>("glm::vec2")  //
            .member<f32>("x")
            .member<f32>("y");

        world_
            .component<glm::vec3>("glm::vec3")  //
            .member<f32>("x")
            .member<f32>("y")
            .member<f32>("z");

        world_
            .component<glm::vec4>("glm::vec4")  //
            .member<f32>("x")
            .member<f32>("y")
            .member<f32>("z")
            .member<f32>("w");

        world_
            .component<glm::mat3>("glm::mat3")  //
            .member<glm::vec3>("col0")
            .member<glm::vec3>("col1")
            .member<glm::vec3>("col2");

        world_
            .component<glm::mat4>("glm::mat4")  //
            .member<glm::vec4>("col0")
            .member<glm::vec4>("col1")
            .member<glm::vec4>("col2")
            .member<glm::vec4>("col3");

        world_.component<std::string>("std::string")
            .opaque(flecs::String)
            .serialize([](const flecs::serializer *s, const std::string *data) {
                const char *str = data->c_str();
                return s->value(flecs::String, &str);
            })
            .assign_string([](std::string *data, const char *value) { *data = value; });

        // SETUP REFLECTION
        Component::reflect_all(world_);

        // SETUP SYSTEMS
        world_.system<Component::PerspectiveCamera, Component::Transform, Component::Camera>()
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

        world_.system<Component::RenderableModel, Component::Transform, Component::RenderableModel>()
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
};
}  // namespace lr::Module
