#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void InspectorPanel::update(this InspectorPanel &self) {
    auto &app = EditorApp::get();
    auto active_tool = app.layout.active_tool;

    ImGui::Begin(self.name.data());
    auto region = ImGui::GetContentRegionAvail();

    auto query = app.world.ecs.query<Component::EditorSelected>();
    query.each([&region](flecs::entity e, Component::EditorSelected) {
        if (ImGui::Button(e.name().c_str(), ImVec2(region.x, 0))) {
            // TODO: Rename entity
        }

        e.each([&e](flecs::id component_id) {
            auto world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            auto component_entity = component_id.entity();
            if (!component_entity.has<flecs::Struct>()) {
                return;
            }

            auto component_name = component_entity.name();
            auto component_data = component_entity.get<flecs::Struct>();
            i32 member_count = ecs_vec_count(&component_data->members);
            if (ImGui::CollapsingHeader(component_name, nullptr)) {
                ImGui::PushID(static_cast<i32>(component_id));
                ImGui::BeginTable(
                    "entity_props",
                    2,
                    ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInner
                        | ImGuiTableFlags_BordersOuterH);
                ImGui::TableSetupColumn("label", 0, 0.5f);
                ImGui::TableSetupColumn("property", ImGuiTableColumnFlags_WidthStretch);

                auto members = static_cast<ecs_member_t *>(ecs_vec_first(&component_data->members));
                auto ptr = static_cast<u8 *>(e.get_mut(component_id));
                ImGui::PushID(ptr);
                for (i32 i = 0; i < member_count; i++) {
                    const auto &member = members[i];

                    // Draw prop label
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y * 0.5f);
                    ImGui::TextUnformatted(member.name);
                    ImGui::TableNextColumn();

                    // Draw prop

                    ImGui::PushID(i);
                    auto member_type = flecs::entity(world, member.type);
                    auto member_offset = member.offset;

                    if (member_type == flecs::F32) {
                        auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                        ImGui::DragFloat("", v);
                    } else if (member_type == flecs::I32) {
                        auto v = reinterpret_cast<i32 *>(ptr + member_offset);
                        ImGui::DragInt("", v);
                    } else if (member_type == world.entity<glm::vec2>()) {
                        auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                        ImGui::DragFloat2("", v);
                    } else if (member_type == world.entity<glm::vec3>()) {
                        auto v = reinterpret_cast<glm::vec3 *>(ptr + member_offset);
                        LRGui::DragXYZ(*v);
                    } else if (member_type == world.entity<glm::vec4>()) {
                        auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                        ImGui::DragFloat4("", v);
                    } else if (member_type == flecs::String) {
                        auto v = reinterpret_cast<const c8 *>(ptr + member_offset);
                        ImGui::Text(member.name, v);
                    }

                    ImGui::PopID();
                }

                ImGui::PopID();
                ImGui::EndTable();
                ImGui::PopID();
            }
        });
    });

    switch (active_tool) {
        case ActiveTool::Cursor:
            break;
        case ActiveTool::Atmosphere: {
            ImGui::SeparatorText(LRED_ICON_SUN "  Atmosphere");
            if (ImGui::DragFloat2("Sun Rotation", &self.sun_dir.x, 0.5f)) {
                auto rad = glm::radians(self.sun_dir);
                glm::vec3 direction = { glm::cos(rad.x) * glm::cos(rad.y), glm::sin(rad.y), glm::sin(rad.x) * glm::cos(rad.y) };
                app.world_render_pipeline.world_data.sun.direction = glm::normalize(direction);
            }
            break;
        }
    }

    ImGui::End();
}

}  // namespace lr
