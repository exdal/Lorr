#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/Memory/Stack.hh"
#include "Engine/World/Components.hh"

namespace lr {
SceneBrowserPanel::SceneBrowserPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void SceneBrowserPanel::update(this SceneBrowserPanel &self) {
    ZoneScoped;

    auto &world = EditorApp::get().world;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable;
    flags |= ImGuiTableFlags_Reorderable;
    flags |= ImGuiTableFlags_Hideable;
    flags |= ImGuiTableFlags_Sortable;
    flags |= ImGuiTableFlags_RowBg;
    flags |= ImGuiTableFlags_Borders;
    flags |= ImGuiTableFlags_NoBordersInBody;
    flags |= ImGuiTableFlags_ScrollX;
    flags |= ImGuiTableFlags_ScrollY;
    flags |= ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0, 0.0, 0.0, 0.0));
    if (ImGui::BeginTable("scene_entity_list", 1, flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f);

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        if (world.active_scene.has_value()) {
            auto &scene = world.scene_at(world.active_scene.value());
            scene.children([&](flecs::entity e) {
                memory::ScopedStack stack;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TableSetColumnIndex(0);
                std::string_view entity_name = {};
                entity_name = stack.format("{}  {}", Icon::fa::cube, e.name().c_str());

                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                if (ImGui::Selectable(entity_name.data(), e.has<Component::EditorSelected>(), selectable_flags)) {
                    world.ecs.each([](flecs::entity c, Component::EditorSelected) { c.remove<Component::EditorSelected>(); });
                    e.add<Component::EditorSelected>();
                }
            });
        }

        if (ImGui::BeginPopupContextWindow("create_ctxwin", ImGuiPopupFlags_AnyPopup | 1)) {
            if (ImGui::BeginMenu("Create...")) {
                if (ImGui::MenuItem("Entity", nullptr, false, world.active_scene.has_value())) {
                    ImGui::OpenPopup("create_entity_popup");
                    ImGui::OpenPopup("fuck");
                }

                if (ImGui::BeginPopup("fuck")) {
                    ImGui::TextUnformatted("wow");
                    ImGui::EndPopup();
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("create_entity_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static std::string entity_name = {};
            ImGui::InputText("Name for Entity", &entity_name);

            if (entity_name.empty()) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                auto &scene = world.scene_at(world.active_scene.value());
                scene.create_entity(entity_name);

                ImGui::CloseCurrentPopup();
                entity_name.clear();
            }

            if (entity_name.empty()) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleColor();

    ImGui::End();
}

}  // namespace lr
