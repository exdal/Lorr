#include "Editor/Panel/SceneBrowserPanel.hh"

#include "Editor/EditorApp.hh"

#include "Engine/Memory/Stack.hh"
#include "Engine/Scene/ECSModule/Core.hh"

#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace lr {
SceneBrowserPanel::SceneBrowserPanel(std::string name_, bool open_): PanelI(std::move(name_), open_) {}

void SceneBrowserPanel::render(this SceneBrowserPanel &self) {
    ZoneScoped;

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

        auto &app = EditorApp::get();
        if (app.active_scene_uuid.has_value()) {
            self.draw_hierarchy();
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleColor();

    ImGui::End();
}

void SceneBrowserPanel::draw_hierarchy(this SceneBrowserPanel &) {
    ZoneScoped;

    auto &app = EditorApp::get();
    auto *scene = app.asset_man.get_scene(app.active_scene_uuid.value());

    scene->get_root().children([&](flecs::entity e) {
        memory::ScopedStack stack;
        if (e.has<ECS::Hidden>()) {
            return;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::TableSetColumnIndex(0);
        auto entity_name = stack.format("{}  {}", ICON_MDI_CUBE, e.name().c_str());

        ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
        ImGui::PushID(static_cast<i32>(e.raw_id()));
        if (ImGui::Selectable(entity_name.data(), e == app.selected_entity, selectable_flags)) {
            app.selected_entity = e;
        }
        ImGui::PopID();
    });

    if (ImGui::BeginPopupContextWindow("create_ctxwin", ImGuiPopupFlags_AnyPopup | 1)) {
        if (ImGui::BeginMenu("Create...")) {
            if (ImGui::MenuItem("Entity")) {
                auto created_entity = scene->create_entity();
                created_entity.set<ECS::Transform>({});
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Model")) {
                auto created_entity = scene->create_entity();
                created_entity.set<ECS::Transform>({});
                created_entity.set<ECS::RenderingModel>({});
            }

            if (ImGui::MenuItem("Directional Light")) {
                auto created_entity = scene->create_entity();
                created_entity.set<ECS::Transform>({});
                created_entity.set<ECS::DirectionalLight>({});
                created_entity.set<ECS::Atmosphere>({});
            }

            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
}

} // namespace lr
