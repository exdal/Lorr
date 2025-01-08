#pragma once

#include "Engine/Asset/UUID.hh"

#include <flecs.h>

namespace lr::ECS {
struct ComponentWrapper {
    using Member = std::variant<
        std::monostate,
        f32 *,
        i32 *,
        u32 *,
        i64 *,
        u64 *,
        glm::vec2 *,
        glm::vec3 *,
        glm::vec4 *,
        glm::mat4 *,
        glm::quat *,
        std::string *,
        UUID *>;

    flecs::entity component_entity = {};
    std::string path = {};
    std::string_view name = {};
    const flecs::Struct *struct_data = nullptr;
    usize member_count = 0;
    ecs_member_t *members = nullptr;
    u8 *members_data = nullptr;

    inline ComponentWrapper(flecs::entity &holder_, flecs::id &comp_id_) {
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
    inline void for_each(this ComponentWrapper &self, const FuncT &fn) {
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
            } else if (member_type == world.entity<glm::mat4>()) {
                data = reinterpret_cast<glm::mat4 *>(self.members_data + member.offset);
            } else if (member_type == world.entity<glm::quat>()) {
                data = reinterpret_cast<glm::quat *>(self.members_data + member.offset);
            } else if (member_type == world.entity<std::string>()) {
                data = reinterpret_cast<std::string *>(self.members_data + member.offset);
            } else if (member_type == world.entity<UUID>()) {
                data = reinterpret_cast<UUID *>(self.members_data + member.offset);
            } else {
                LOG_FATAL("Trying to access unknown component type!");
                return;
            }

            fn(i, member_name, data);
        }
    }
};
}  // namespace lr::ECS
