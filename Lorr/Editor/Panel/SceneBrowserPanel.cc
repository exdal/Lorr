#include "Editor/Panel/SceneBrowserPanel.hh"

#include "Editor/EditorApp.hh"

#include "Engine/Memory/Stack.hh"
#include "Engine/World/ECSModule/Core.hh"

namespace lr {
SceneBrowserPanel::SceneBrowserPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void SceneBrowserPanel::update(this SceneBrowserPanel &self) {
    ZoneScoped;

    auto &app = EditorApp::get();
    auto &asset_man = app.asset_man;
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

    bool open_create_entity_popup = false;
    if (ImGui::BeginTable("scene_entity_list", 1, flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f);

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        if (app.active_scene_id.has_value()) {
            auto *scene = asset_man.get_scene(app.active_scene_id.value());
            scene->get_root().children([&](flecs::entity e) {
                if (e.has<ECS::Hidden>()) {
                    return;
                }

                memory::ScopedStack stack;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TableSetColumnIndex(0);
                auto entity_name = stack.format("{}  {}", Icon::fa::cube, e.name().c_str());

                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                if (ImGui::Selectable(entity_name.data(), e.has<ECS::EditorSelected>(), selectable_flags)) {
                    scene->get_world().each([](flecs::entity c, ECS::EditorSelected) { c.remove<ECS::EditorSelected>(); });
                    e.add<ECS::EditorSelected>();
                }
            });
        }

        if (ImGui::BeginPopupContextWindow("create_ctxwin", ImGuiPopupFlags_AnyPopup | 1)) {
            if (ImGui::BeginMenu("Create...")) {
                if (ImGui::MenuItem("Entity", nullptr, false, app.active_scene_id.has_value())) {
                    open_create_entity_popup = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        if (open_create_entity_popup) {
            ImGui::OpenPopup("###create_entity_popup");
        }

        if (ImGui::BeginPopupModal("Create Entity...###create_entity_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static std::string entity_name = {};
            ImGui::InputText("Name for Entity", &entity_name);

            if (ImGui::ButtonEx("OK", ImVec2(120, 0), ImGuiItemFlags_Disabled)) {
                auto *scene = asset_man.get_scene(app.active_scene_id.value());
                auto created_entity = scene->create_entity(entity_name);
                created_entity.set<ECS::Transform>({});

                ImGui::CloseCurrentPopup();
                entity_name.clear();
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
