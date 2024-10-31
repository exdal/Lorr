#pragma once

#include "Engine/Asset/Identifier.hh"

#include <flecs.h>
#include <glm/gtx/quaternion.hpp>

template<>
struct std::formatter<flecs::string_view> : formatter<string_view> {
    template<typename FormatContext>
    constexpr auto format(const flecs::string_view &v, FormatContext &ctx) const {
        return std::format_to(ctx.out(), "{}", std::string_view(v.c_str(), v.length()));
    }
};

namespace lr::Component {
struct Transform {
    constexpr static auto ICON = Icon::fa::up_down_left_right;

    glm::vec3 position = { 0.0, 0.0, 0.0 };
    glm::vec3 scale = { 1.0, 1.0, 1.0 };
    glm::vec3 rotation = {};
    glm::mat4 matrix = {};

    static void reflect(flecs::world &w) {
        w.component<Transform>()  //
            .member<glm::vec3, Transform>("position", &Transform::position)
            .member<glm::vec3, Transform>("scale", &Transform::scale)
            .member<glm::vec3, Transform>("rotation", &Transform::rotation);
    }
};

struct Camera {
    constexpr static auto ICON = Icon::fa::camera;

    glm::quat orientation = {};
    glm::mat4 projection = {};
    glm::vec3 velocity = {};
    f32 fov = 60.0f;
    f32 aspect_ratio = 1.777f;

    static void reflect(flecs::world &w) {
        w.component<Camera>()  //
            .member<glm::vec3, Camera>("velocity", &Camera::velocity)
            .member<f32, Camera>("fov", &Camera::fov)
            .member<f32, Camera>("aspect_ratio", &Camera::aspect_ratio);
    }
};

struct RenderableModel {
    constexpr static auto ICON = Icon::fa::cube;

    Identifier identifier = {};

    static void reflect(flecs::world &w) {
        w.component<RenderableModel>()  //
            .member<Identifier, RenderableModel>("identifier", &RenderableModel::identifier);
    }
};

struct DirectionalLight {
    constexpr static auto ICON = Icon::fa::sun;

    f32 intensity = 10.0f;
    static void reflect(flecs::world &w) {
        w.component<DirectionalLight>()  //
            .member<f32, DirectionalLight>("intensity", &DirectionalLight::intensity);
    }
};

/// TAGS ///

struct ActiveCamera {
    static void reflect(flecs::world &w) { w.component<ActiveCamera>(); }
};

struct EditorSelected {
    static void reflect(flecs::world &w) { w.component<EditorSelected>(); }
};

struct PerspectiveCamera {
    static void reflect(flecs::world &w) { w.component<PerspectiveCamera>(); }
};

struct OrthographicCamera {
    static void reflect(flecs::world &w) { w.component<OrthographicCamera>(); }
};

constexpr static std::tuple<  //
    Transform,
    Camera,
    RenderableModel,
    DirectionalLight,
    ActiveCamera,
    EditorSelected,
    PerspectiveCamera,
    OrthographicCamera>
    ALL_COMPONENTS;

template<typename T>
constexpr static void do_reflect(flecs::world &w) {
    T::reflect(w);
}

constexpr static void reflect_all(flecs::world &w) {
    std::apply(
        [&w](const auto &...args) {  //
            (do_reflect<std::decay_t<decltype(args)>>(w), ...);
        },
        ALL_COMPONENTS);
}

struct Wrapper {
    using Member = std::variant<std::monostate, f32 *, i32 *, u32 *, i64 *, u64 *, glm::vec2 *, glm::vec3 *, glm::vec4 *, std::string *>;

    flecs::entity component_entity = {};
    std::string path = {};
    std::string_view name = {};
    const flecs::Struct *struct_data = nullptr;
    usize member_count = 0;
    ecs_member_t *members = nullptr;
    u8 *members_data = nullptr;

    inline Wrapper(flecs::entity &holder_, flecs::id &comp_id_) {
        component_entity = comp_id_.entity();
        path = component_entity.path();
        name = { component_entity.name(), component_entity.name().length() };

        if (!has_component()) {
            return;
        }

        struct_data = component_entity.get<flecs::Struct>();
        member_count = ecs_vec_count(&struct_data->members);
        members = static_cast<ecs_member_t *>(ecs_vec_first(&struct_data->members));
        members_data = static_cast<u8 *>(holder_.get_mut(comp_id_));
    }

    inline bool has_component() { return component_entity.has<flecs::Struct>(); }
    template<typename FuncT>
    inline void for_each(this Wrapper &self, const FuncT &fn) {
        ZoneScoped;

        auto world = self.component_entity.world();
        for (usize i = 0; i < self.member_count; i++) {
            const auto &member = self.members[i];
            std::string_view member_name(member.name);
            Member data = std::monostate{};
            auto member_type = flecs::entity(world, member.type);

            if (member_type == flecs::F32) {
                data = reinterpret_cast<f32 *>(self.members_data + member.offset);
            } else if (member_type == flecs::I32) {
                data = reinterpret_cast<i32 *>(self.members_data + member.offset);
            } else if (member_type == flecs::U32) {
                data = reinterpret_cast<u32 *>(self.members_data + member.offset);
            } else if (member_type == flecs::I64) {
                data = reinterpret_cast<i64 *>(self.members_data + member.offset);
            } else if (member_type == flecs::I64) {
                data = reinterpret_cast<u64 *>(self.members_data + member.offset);
            } else if (member_type == world.entity<glm::vec2>()) {
                data = reinterpret_cast<glm::vec2 *>(self.members_data + member.offset);
            } else if (member_type == world.entity<glm::vec3>()) {
                data = reinterpret_cast<glm::vec3 *>(self.members_data + member.offset);
            } else if (member_type == world.entity<glm::vec4>()) {
                data = reinterpret_cast<glm::vec4 *>(self.members_data + member.offset);
            } else if (member_type == world.entity<std::string>()) {
                data = reinterpret_cast<std::string *>(self.members_data + member.offset);
            }

            fn(i, member_name, data);
        }
    }
};

}  // namespace lr::Component
