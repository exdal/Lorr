#include "World.hh"

#include "Components.hh"

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

    self.ecs.component<c32>("c32")
        .opaque(flecs::String)
        .serialize([](const flecs::serializer *s, const c32 *data) { return s->value(flecs::String, &data); })
        .assign_string([](c32 *data, const char *value) { *data = *reinterpret_cast<const c32 *>(value); });

    self.ecs.component<std::string>("std::string")
        .opaque(flecs::String)
        .serialize([](const flecs::serializer *s, const std::string *data) {
            const char *str = data->c_str();
            return s->value(flecs::String, &str);
        })
        .assign_string([](std::string *data, const char *value) { *data = value; });

    // SETUP REFLECTION
    Component::Transform::reflect(self.ecs);
    Component::Camera::reflect(self.ecs);

    // SETUP PREFABS
    self.ecs
        .prefab<Prefab::PerspectiveCamera>()  //
        .add<Prefab::PerspectiveCamera>()
        .set<Component::Icon>({ "\uf030" })
        .set<Component::Transform>({})
        .set<Component::Camera>({});

    self.ecs
        .prefab<Prefab::OrthographicCamera>()  //
        .add<Prefab::OrthographicCamera>()
        .set<Component::Icon>({ "\uf030" })
        .set<Component::Transform>({})
        .set<Component::Camera>({});

    // SETUP SYSTEMS
    self.ecs.system<Prefab::PerspectiveCamera, Component::Transform, Component::Camera>()
        .kind(flecs::OnUpdate)
        .each([](flecs::iter &it, usize, Prefab::PerspectiveCamera, Component::Transform &t, Component::Camera &c) {
            t.rotation.x = std::fmod(t.rotation.x, 360.0f);
            t.rotation.y = glm::clamp(t.rotation.y, -89.0f, 89.0f);

            auto inv_orient = glm::conjugate(c.orientation);
            t.position += glm::vec3(inv_orient * glm::vec4(c.velocity * it.delta_time(), 0.0f));

            c.projection = glm::perspectiveLH(glm::radians(c.fov), c.aspect_ratio, 0.1f, 10000.0f);
            c.projection[1][1] *= -1;

            c.orientation = glm::angleAxis(glm::radians(t.rotation.x), glm::vec3(0.0f, -1.0f, 0.0f));
            c.orientation = glm::angleAxis(glm::radians(t.rotation.y), glm::vec3(1.0f, 0.0f, 0.0f)) * c.orientation;
            c.orientation = glm::angleAxis(glm::radians(t.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f)) * c.orientation;
            c.orientation = glm::normalize(c.orientation);

            glm::mat4 rotation_mat = glm::toMat4(c.orientation);
            glm::mat4 translation_mat = glm::translate(glm::mat4(1.f), -t.position);
            t.matrix = rotation_mat * translation_mat;
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

void World::set_active_scene(this World &self, SceneID scene_id) {
    ZoneScoped;

    usize scene_idx = static_cast<usize>(scene_id);
    if (scene_id != SceneID::Invalid && scene_idx < self.scenes.size()) {
        self.active_scene = scene_id;
    }
}

}  // namespace lr
