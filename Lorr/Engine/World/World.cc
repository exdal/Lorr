#include "World.hh"

#include "Engine/Core/Application.hh"
#include "Engine/OS/File.hh"
#include "Engine/World/Components.hh"

#include <simdjson.h>

namespace lr {
template<typename T>
concept HasIcon = requires { T::ICON; };

template<typename T>
concept NotHasIcon = !requires { T::ICON; };

template<HasIcon T>
constexpr static auto do_get_icon() {
    return T::ICON;
}

template<NotHasIcon T>
constexpr static Icon::detail::icon_t do_get_icon() {
    return nullptr;
}

template<>
struct Handle<World>::Impl {
    flecs::world ecs{};
    flecs::entity root = {};
    ls::option<SceneID> active_scene = ls::nullopt;

    ankerl::unordered_dense::map<flecs::id_t, Icon::detail::icon_t> component_icons = {};

    std::vector<GPUModel> gpu_models = {};
};

auto World::create(const std::string &name) -> ls::option<World> {
    ZoneScoped;

    auto impl = new Impl;
    auto self = World(impl);

    //  ── SETUP ECS ───────────────────────────────────────────────────────
    impl->root = impl->ecs.entity(name.c_str());
    impl->ecs
        .component<glm::vec2>("glm::vec2")  //
        .member<f32>("x")
        .member<f32>("y");

    impl->ecs
        .component<glm::vec3>("glm::vec3")  //
        .member<f32>("x")
        .member<f32>("y")
        .member<f32>("z");

    impl->ecs
        .component<glm::vec4>("glm::vec4")  //
        .member<f32>("x")
        .member<f32>("y")
        .member<f32>("z")
        .member<f32>("w");

    impl->ecs
        .component<glm::mat3>("glm::mat3")  //
        .member<glm::vec3>("col0")
        .member<glm::vec3>("col1")
        .member<glm::vec3>("col2");

    impl->ecs
        .component<glm::mat4>("glm::mat4")  //
        .member<glm::vec4>("col0")
        .member<glm::vec4>("col1")
        .member<glm::vec4>("col2")
        .member<glm::vec4>("col3");

    impl->ecs.component<std::string>("std::string")
        .opaque(flecs::String)
        .serialize([](const flecs::serializer *s, const std::string *data) {
            const char *str = data->c_str();
            return s->value(flecs::String, &str);
        })
        .assign_string([](std::string *data, const char *value) { *data = value; });

    impl->ecs.component<UUID>("uuid")
        .opaque(flecs::String)
        .serialize([](const flecs::serializer *s, const UUID *data) {
            auto str = data->str();
            return s->value(flecs::String, str.c_str());
        })
        .assign_string([](UUID *data, const char *value) { *data = UUID::from_string(std::string_view(value)).value(); });

    Component::reflect_all(impl->ecs);

    //  ── SETUP DEFAULT SYSTEMS ───────────────────────────────────────────
    impl->ecs.system<Component::PerspectiveCamera, Component::Transform, Component::Camera>()
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

    impl->ecs
        .system<Component::Transform>()  //
        .kind(flecs::OnUpdate)
        .without<Component::Camera>()
        .each([](flecs::iter &, usize, Component::Transform &t) {
            auto rotation = glm::radians(t.rotation);

            t.matrix = glm::translate(glm::mat4(1.0), t.position);
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
            t.matrix *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
            t.matrix *= glm::scale(glm::mat4(1.0), t.scale);
        });

    // initialize component icons
    auto get_icon_of = [&](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        flecs::entity e = impl->ecs.entity<T>();
        impl->component_icons.emplace(e.raw_id(), do_get_icon<T>());
    };

    std::apply(
        [&](const auto &...args) {  //
            (get_icon_of(std::decay_t<decltype(args)>()), ...);
        },
        Component::ALL_COMPONENTS);

    return self;
}

auto World::create_from_file(const fs::path &path) -> ls::option<World> {
    ZoneScoped;

    ZoneScoped;
    namespace sj = simdjson;

    const fs::path &project_root = path.parent_path();
    File file(path, FileAccess::Read);
    if (!file) {
        LOG_ERROR("Failed to read '{}'!", path);
        return ls::nullopt;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse world file! {}", sj::error_message(doc.error()));
        return ls::nullopt;
    }

    auto name_json = doc["name"];
    if (name_json.error() || !name_json.is_string()) {
        LOG_ERROR("World file must have `name` value as string.");
        return ls::nullopt;
    }

    auto world_name = std::string(name_json.get_string().value());
    return World::create(world_name);
}

auto World::destroy() -> void {
    ZoneScoped;

    impl->ecs.quit();

    delete impl;
    impl = nullptr;
}

auto World::begin_frame(WorldRenderer &renderer) -> void {
    ZoneScoped;

    if (!impl->active_scene.has_value()) {
        return;
    }

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto scene = asset_man.get_scene(impl->active_scene.value());

    auto camera_query = impl->ecs  //
                            .query_builder<Component::Transform, Component::Camera>()
                            .with(flecs::ChildOf, scene.root())
                            .build();
    auto model_transform_query = impl->ecs  //
                                     .query_builder<Component::Transform, Component::RenderableModel>()
                                     .with(flecs::ChildOf, scene.root())
                                     .build();

    auto scene_begin_info = renderer.begin_scene_render_data(camera_query.count(), model_transform_query.count());
    if (!scene_begin_info.has_value()) {
        return;
    }

    impl->gpu_models.clear();
    u32 active_camera_index = 0;
    camera_query.each([&](flecs::entity e, Component::Transform &t, Component::Camera &c) {
        auto camera_ptr = reinterpret_cast<GPUCameraData *>(scene_begin_info->cameras_allocation.ptr);
        camera_ptr += c.index;

        camera_ptr->projection_mat = c.projection;
        camera_ptr->view_mat = t.matrix;
        camera_ptr->projection_view_mat = glm::transpose(c.projection * t.matrix);
        camera_ptr->inv_view_mat = glm::inverse(glm::transpose(t.matrix));
        camera_ptr->inv_projection_view_mat = glm::inverse(glm::transpose(c.projection)) * camera_ptr->inv_view_mat;
        camera_ptr->position = t.position;
        camera_ptr->near_clip = c.near_clip;
        camera_ptr->far_clip = c.far_clip;

        if (e.has<Component::ActiveCamera>()) {
            active_camera_index = c.index;
        }
    });

    u32 model_transform_index = 0;
    auto model_transforms_ptr = reinterpret_cast<GPUModelTransformData *>(scene_begin_info->model_transfors_allocation.ptr);
    model_transform_query.each([&](Component::Transform &t, Component::RenderableModel &m) {
        auto rotation = glm::radians(t.rotation);

        glm::mat4 &world_mat = model_transforms_ptr->world_transform_mat;
        world_mat = glm::translate(glm::mat4(1.0), glm::vec3(0.0f));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
        world_mat *= glm::scale(glm::mat4(1.0), t.scale);

        model_transforms_ptr->model_transform_mat = t.matrix;
        model_transforms_ptr++;

        // ──────────────────────────────────────────────────────────────────────
        auto *model = asset_man.get_model(m.model_id);
        auto &gpu_model = impl->gpu_models.emplace_back();

        gpu_model.primitives.reserve(model->primitives.size());
        gpu_model.meshes.reserve(model->meshes.size());

        for (auto &primitive : model->primitives) {
            gpu_model.primitives.push_back({
                .vertex_offset = primitive.vertex_offset,
                .vertex_count = primitive.vertex_count,
                .index_offset = primitive.index_offset,
                .index_count = primitive.index_count,
                .material_index = std::to_underlying(primitive.material_id),
            });
        }

        for (auto &mesh : model->meshes) {
            gpu_model.meshes.push_back({
                .primitive_indices = mesh.primitive_indices,
            });
        }

        gpu_model.transform_index = model_transform_index++;
        gpu_model.vertex_bufffer_id = model->vertex_buffer.id();
        gpu_model.index_buffer_id = model->index_buffer.id();
    });

    scene_begin_info->active_camera = active_camera_index;
    scene_begin_info->materials_buffer_id = asset_man.material_buffer().id();
    scene_begin_info->gpu_models = impl->gpu_models;
    renderer.end_scene_render_data(scene_begin_info.value());
}

auto World::end_frame() -> void {
    ZoneScoped;

    impl->ecs.progress();
}

auto World::set_active_scene(SceneID scene_id) -> void {
    ZoneScoped;

    impl->active_scene = scene_id;
}

auto World::active_scene() -> ls::option<SceneID> {
    ZoneScoped;

    return impl->active_scene;
}

auto World::should_quit() -> bool {
    ZoneScoped;

    return impl->ecs.should_quit();
}

auto World::delta_time() -> f64 {
    return impl->ecs.delta_time();
}

auto World::component_icon(flecs::id_t id) -> Icon::detail::icon_t {
    return impl->component_icons[id];
}

auto World::create_entity(const std::string &name) -> flecs::entity {
    return impl->ecs.entity(name.c_str());
}

auto World::ecs() const -> const flecs::world & {
    return impl->ecs;
}

}  // namespace lr
