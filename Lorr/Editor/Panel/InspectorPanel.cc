#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void InspectorPanel::update(this InspectorPanel &self) {
    auto &app = EditorApp::get();

    ImGui::Begin(self.name.data());
    auto region = ImGui::GetContentRegionAvail();

    auto query = app.world.ecs.query<Component::EditorSelected>();
    query.each([&](flecs::entity e, Component::EditorSelected) {
        if (ImGui::Button(e.name().c_str(), ImVec2(region.x, 0))) {
            // TODO: Rename entity
        }

        e.each([&](flecs::id component_id) {
            auto world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            auto component_entity = component_id.entity();
            if (!component_entity.has<flecs::Struct>()) {
                return;
            }

            auto component_name = component_entity.doc_name();
            auto component_data = component_entity.get<flecs::Struct>();
            i32 member_count = ecs_vec_count(&component_data->members);
            auto name_with_icon = fmt::format("{}  {}", app.component_icons[component_id.raw_id()], component_name);
            if (ImGui::CollapsingHeader(name_with_icon.c_str(), nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
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
                    } else if (member_type == flecs::U32) {
                        auto v = reinterpret_cast<u32 *>(ptr + member_offset);
                        ImGui::DragScalar("", ImGuiDataType_U32, v);
                    } else if (member_type == flecs::I64) {
                        auto v = reinterpret_cast<i64 *>(ptr + member_offset);
                        ImGui::DragScalar("", ImGuiDataType_S64, v);
                    } else if (member_type == flecs::I64) {
                        auto v = reinterpret_cast<u64 *>(ptr + member_offset);
                        ImGui::DragScalar("", ImGuiDataType_U64, v);
                    } else if (member_type == world.entity<glm::vec2>()) {
                        auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                        ImGui::DragFloat2("", v);
                    } else if (member_type == world.entity<glm::vec3>()) {
                        auto v = reinterpret_cast<glm::vec3 *>(ptr + member_offset);
                        LRGui::DragXYZ(*v);
                    } else if (member_type == world.entity<glm::vec4>()) {
                        auto v = reinterpret_cast<f32 *>(ptr + member_offset);
                        ImGui::DragFloat4("", v);
                    } else if (member_type == world.entity<std::string>()) {
                        auto v = reinterpret_cast<std::string *>(ptr + member_offset);

                        ImGui::InputText("", v);
                    }

                    ImGui::PopID();
                }

                ImGui::PopID();
                ImGui::EndTable();
                ImGui::PopID();
            }
        });

        if (ImGui::Button("Add Component", ImVec2(region.x, 0))) {
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    });

    ImGui::End();
}

}  // namespace lr
