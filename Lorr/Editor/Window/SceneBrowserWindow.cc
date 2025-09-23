#include "Editor/Window/SceneBrowserWindow.hh"

#include "Editor/EditorModule.hh"

#include "Engine/Memory/Stack.hh"
#include "Engine/Scene/ECSModule/Core.hh"

#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

#include "Engine/Asset/Asset.hh"
#include "Engine/Core/App.hh"

namespace led {
static auto draw_children(SceneBrowserWindow &self, flecs::entity root) -> void {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &editor = lr::App::mod<EditorModule>();
    auto &active_project = *editor.active_project;
    auto &active_scene_uuid = active_project.active_scene_uuid;
    auto *active_scene = asset_man.get_scene(active_scene_uuid);
    auto &selected_entity = active_project.selected_entity;
    auto &world = active_scene->get_world();

    auto q = world //
                 .query_builder()
                 .with(flecs::ChildOf, root)
                 .build();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    q.each([&](flecs::entity e) {
        lr::memory::ScopedStack stack;
        auto id = e.id();
        auto has_childs = e.world().count(flecs::ChildOf, e) != 0;

        ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanFullWidth;
        if (!has_childs) {
            tree_node_flags |= ImGuiTreeNodeFlags_Leaf;
        }

        auto *entity_name = stack.format_char("{}  {}", ICON_MDI_CUBE, e.name().c_str());
        ImGui::PushID(static_cast<i32>(id));
        auto is_open = ImGui::TreeNodeEx(entity_name, tree_node_flags);
        if (ImGui::IsItemClicked()) {
            selected_entity = e;
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Create children")) {
                auto child_entity = active_scene->create_entity();
                child_entity.child_of(e);
            }

            if (ImGui::MenuItem("Delete")) {
                active_scene->delete_entity(e);
            }

            ImGui::EndPopup();
        }

        if (is_open) {
            draw_children(self, e);

            ImGui::TreePop();
        }

        ImGui::PopID();
    });
}

static auto draw_hierarchy(SceneBrowserWindow &self) -> void {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &editor = lr::App::mod<EditorModule>();
    auto &active_project = *editor.active_project;
    auto &active_scene_uuid = active_project.active_scene_uuid;
    auto *active_scene = asset_man.get_scene(active_scene_uuid);

    draw_children(self, active_scene->get_root());

    const auto popup_flags = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup;
    if (ImGui::BeginPopupContextWindow("create_ctxwin", popup_flags)) {
        if (ImGui::BeginMenu("Create...")) {
            // WARN: This is when we create new entitiy on root level!!!
            if (ImGui::MenuItem("Entity")) {
                auto created_entity = active_scene->create_entity();
                created_entity.set<lr::ECS::Transform>({});
                created_entity.child_of(active_scene->get_root());
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
        flags |= ImGuiTableFlags_NoPadInnerX;
        flags |= ImGuiTableFlags_NoPadOuterX;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0, 0.0, 0.0, 0.0));
        if (ImGui::BeginTable("scene_entity_list", 1, flags)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f);

            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0, 0 });
            auto &editor = lr::App::mod<EditorModule>();
            if (editor.active_project && editor.active_project->active_scene_uuid) {
                draw_hierarchy(self);
            }
            ImGui::PopStyleVar();

            ImGui::EndTable();
        }
        ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace led
