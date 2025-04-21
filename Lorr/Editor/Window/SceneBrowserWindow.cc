#include "Editor/Window/SceneBrowserWindow.hh"

#include "Editor/EditorApp.hh"

#include "Engine/Memory/Stack.hh"
#include "Engine/Scene/ECSModule/Core.hh"

#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace led {
static auto draw_hierarchy(SceneBrowserWindow &) -> void {
    ZoneScoped;

    auto &app = EditorApp::get();
    auto &active_project = *app.active_project;
    auto &active_scene_uuid = active_project.active_scene_uuid;
    auto *active_scene = app.asset_man.get_scene(active_scene_uuid);
    auto &selected_entity = active_project.selected_entity;

    active_scene->get_root().children([&](flecs::entity e) {
        lr::memory::ScopedStack stack;
        if (e.has<lr::ECS::Hidden>()) {
            return;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::TableSetColumnIndex(0);
        auto *entity_name = stack.format_char("{}  {}", ICON_MDI_CUBE, e.name().c_str());

        ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
        ImGui::PushID(static_cast<i32>(e.raw_id()));
        if (ImGui::Selectable(entity_name, e == selected_entity, selectable_flags)) {
            selected_entity = e;
        }
        ImGui::PopID();
    });

    if (ImGui::BeginPopupContextWindow("create_ctxwin", ImGuiPopupFlags_AnyPopup | 1)) {
        if (ImGui::BeginMenu("Create...")) {
            if (ImGui::MenuItem("Entity")) {
                auto created_entity = active_scene->create_entity();
                created_entity.set<lr::ECS::Transform>({});
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Model")) {
                auto created_entity = active_scene->create_entity();
                created_entity.set<lr::ECS::Transform>({});
                created_entity.set<lr::ECS::RenderingModel>({});
            }

            if (ImGui::MenuItem("Directional Light")) {
                auto created_entity = active_scene->create_entity();
                created_entity.set<lr::ECS::Transform>({});
                created_entity.set<lr::ECS::DirectionalLight>({});
                created_entity.set<lr::ECS::Atmosphere>({});
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
}

SceneBrowserWindow::SceneBrowserWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {}

void SceneBrowserWindow::render(this SceneBrowserWindow &self) {
    ZoneScoped;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    if (ImGui::Begin(self.name.data())) {
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

            auto &app = EditorApp::get();
            if (app.active_project && app.active_project->active_scene_uuid) {
                draw_hierarchy(self);
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleColor();

        ImGui::End();
    }
}

} // namespace led
