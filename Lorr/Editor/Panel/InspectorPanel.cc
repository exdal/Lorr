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

            Component::Wrapper component(e, component_id);
            if (!component.has_component()) {
                return;
            }

            auto name_with_icon = std::format("{}  {}", app.component_icons[component_id.raw_id()], component.name);
            if (ImGui::CollapsingHeader(name_with_icon.c_str(), nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PushID(static_cast<i32>(component_id));
                ImGui::BeginTable(
                    "entity_props",
                    2,
                    ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInner
                        | ImGuiTableFlags_BordersOuterH);
                ImGui::TableSetupColumn("label", 0, 0.5f);
                ImGui::TableSetupColumn("property", ImGuiTableColumnFlags_WidthStretch);

                ImGui::PushID(component.members_data);
                component.for_each([&](usize &i, std::string_view member_name, Component::Wrapper::Member &member) {
                    // Draw prop label
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y * 0.5f);
                    ImGui::TextUnformatted(member_name.data());
                    ImGui::TableNextColumn();

                    ImGui::PushID(static_cast<i32>(i));
                    std::visit(
                        match{
                            [](const auto &) {},
                            [](f32 *v) { ImGui::DragFloat("", v); },
                            [](i32 *v) { ImGui::DragInt("", v); },
                            [](u32 *v) { ImGui::DragScalar("", ImGuiDataType_U32, v); },
                            [](i64 *v) { ImGui::DragScalar("", ImGuiDataType_S64, v); },
                            [](u64 *v) { ImGui::DragScalar("", ImGuiDataType_U64, v); },
                            [](glm::vec2 *v) { LRGui::DragXY(*v); },
                            [](glm::vec3 *v) { LRGui::DragXYZ(*v); },
                            [](std::string *v) { ImGui::InputText("", v); },
                        },
                        member);
                    ImGui::PopID();
                });

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
