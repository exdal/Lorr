#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

namespace lr {
SceneBrowserPanel::SceneBrowserPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void draw_gradient_shadow_bottom(const float scale) {
    const auto draw_list = ImGui::GetWindowDrawList();
    const auto pos = ImGui::GetWindowPos();
    const auto window_height = ImGui::GetWindowHeight();
    const auto window_width = ImGui::GetWindowWidth();

    const ImRect bb(0, scale, pos.x + window_width, window_height + pos.y);
    draw_list->AddRectFilledMultiColor(bb.Min, bb.Max, IM_COL32(20, 20, 20, 0), IM_COL32(20, 20, 20, 0), IM_COL32(20, 20, 20, 255), IM_COL32(20, 20, 20, 255));
}

void SceneBrowserPanel::update(this SceneBrowserPanel &self) {
    ZoneScoped;

    static bool create_entity_open = false;
    auto &app = EditorApp::get();
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

        if (app.active_scene.has_value()) {
            auto &scene = app.scene_at(app.active_scene.value());
            scene.children([&](flecs::entity e) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TableSetColumnIndex(0);

                ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                if (ImGui::Selectable(e.name().c_str(), e.has<EditorSelectedComp>(), selectable_flags)) {
                    app.ecs.each([](flecs::entity c, EditorSelectedComp) { c.remove<EditorSelectedComp>(); });
                    e.add<EditorSelectedComp>();
                }
            });
        }

        if (ImGui::BeginPopupContextWindow("create_ctxwin")) {
            if (ImGui::BeginMenu("Create...")) {
                if (ImGui::MenuItem("Entity", nullptr, false, app.active_scene.has_value())) {
                    create_entity_open = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleColor();

    if (create_entity_open) {
        ImGui::OpenPopup("###create_entity");
    }

    if (ImGui::BeginPopupModal("Create an entity...###create_entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char entity_name[64] = {};
        std::string_view entity_name_sv = entity_name;
        ImGui::InputText("Name for Entity", entity_name, 64);

        if (entity_name_sv.empty()) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            auto &scene = app.scene_at(app.active_scene.value());
            scene.create_entity(entity_name);

            ImGui::CloseCurrentPopup();

            std::memset(entity_name, 0, count_of(entity_name));
        }

        if (entity_name_sv.empty()) {
            ImGui::EndDisabled();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();

            std::memset(entity_name, 0, count_of(entity_name));
        }

        ImGui::EndPopup();
        create_entity_open = false;
    }

    ImGui::End();
}

}  // namespace lr
