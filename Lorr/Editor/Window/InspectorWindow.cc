#include "Editor/Window/InspectorWindow.hh"

#include "Editor/EditorApp.hh"

#include "Engine/Scene/ECSModule/ComponentWrapper.hh"
#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace led {
auto inspect_asset(lr::UUID &uuid) -> bool {
    lr::memory::ScopedStack stack;
    auto &app = lr::Application::get();
    auto *asset = app.asset_man.get_asset(uuid);
    if (!asset) {
        ImGui::TextUnformatted("Drop a model here.");
        uuid = {};
    } else {
        const auto &asset_name = asset->path.filename();
        ImGui::TextUnformatted(stack.format_char("Name: {}", asset_name));
        ImGui::TextUnformatted(stack.format_char("UUID: {}", uuid.str()));
        ImGui::TextUnformatted(stack.format_char("Type: {}", app.asset_man.to_asset_type_sv(asset->type)));
    }

    return false;
}

static auto draw_inspector(InspectorWindow &) -> void {
    auto &app = led::EditorApp::get();
    auto &active_project = *app.active_project;
    auto &active_scene_uuid = active_project.active_scene_uuid;
    auto *active_scene = app.asset_man.get_scene(active_scene_uuid);
    auto &selected_entity = active_project.selected_entity;
    auto region = ImGui::GetContentRegionAvail();

    static std::string new_entity_name = {};
    if (ImGui::Button(selected_entity.name().c_str(), ImVec2(region.x, 0))) {
        new_entity_name = std::string(selected_entity.name().c_str(), selected_entity.name().length());
        ImGui::OpenPopup("###rename_entity");
    }

    if (ImGui::BeginPopupModal("Rename entity to...###rename_entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("", &new_entity_name);

        auto entity_exists = active_scene->find_entity(new_entity_name) || new_entity_name.empty();
        if (entity_exists) {
            ImGui::TextColored({ 1.0, 0.0, 0.0, 1.0 }, "Entity with same already exists.");
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("OK")) {
            selected_entity.set_name(new_entity_name.c_str());
            ImGui::CloseCurrentPopup();
        }

        if (entity_exists) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    std::vector<flecs::id> removing_components = {};
    selected_entity.each([&](flecs::id component_id) {
        lr::memory::ScopedStack stack;

        const auto &ecs_world = selected_entity.world();
        if (!component_id.is_entity()) {
            return;
        }

        lr::ECS::ComponentWrapper component(selected_entity, component_id);
        if (!component.has_component()) {
            return;
        }

        auto name_with_icon = stack.format_char("{}", component.name);
        ImGui::PushID(static_cast<i32>(component_id));
        if (ImGui::CollapsingHeader(name_with_icon, nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BeginTable(
                "entity_props",
                2,
                ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_NoSavedSettings
            );
            ImGui::TableSetupColumn("label", 0, 0.5f);
            ImGui::TableSetupColumn("property", ImGuiTableColumnFlags_WidthStretch);

            ImGui::PushID(component.members_data);
            component.for_each([&](usize &i, std::string_view member_name, lr::ECS::ComponentWrapper::Member &member) {
                // Draw prop label
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
                ImGui::TextUnformatted(member_name.data(), member_name.data() + member_name.length());
                ImGui::TableNextColumn();

                bool component_modified = false;
                ImGui::PushID(static_cast<i32>(i));
                std::visit(
                    ls::match{
                        [](const auto &) {},
                        [&](f32 *v) { component_modified |= ImGui::drag_vec(0, v, 1, ImGuiDataType_Float); },
                        [&](i32 *v) { component_modified |= ImGui::drag_vec(0, v, 1, ImGuiDataType_S32); },
                        [&](u32 *v) { component_modified |= ImGui::drag_vec(0, v, 1, ImGuiDataType_U32); },
                        [&](i64 *v) { component_modified |= ImGui::drag_vec(0, v, 1, ImGuiDataType_S64); },
                        [&](u64 *v) { component_modified |= ImGui::drag_vec(0, v, 1, ImGuiDataType_U64); },
                        [&](glm::vec2 *v) { component_modified |= ImGui::drag_vec(0, glm::value_ptr(*v), 2, ImGuiDataType_Float); },
                        [&](glm::vec3 *v) { component_modified |= ImGui::drag_vec(0, glm::value_ptr(*v), 3, ImGuiDataType_Float); },
                        [&](glm::vec4 *v) { component_modified |= ImGui::drag_vec(0, glm::value_ptr(*v), 4, ImGuiDataType_Float); },
                        [](std::string *v) { ImGui::InputText("", v); },
                        [&](lr::UUID *v) { component_modified |= inspect_asset(*v); },
                    },
                    member
                );
                ImGui::PopID();

                if (component_modified) {
                    selected_entity.modified(component_id.entity());
                }
            });

            ImGui::PopID();
            ImGui::EndTable();

            if (ImGui::Button("Remove Component", ImVec2(region.x, 0))) {
                removing_components.push_back(component_id);
            }
        }
        ImGui::PopID();
    });

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    if (ImGui::Button("Add Component", ImVec2(region.x, 0))) {
        ImGui::OpenPopup("add_component");
    }

    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
    ImGui::SetNextWindowSize({ region.x, 0 });
    if (ImGui::BeginPopup("add_component")) {
        auto &entity_db = active_scene->get_entity_db();
        auto all_components = entity_db.get_components();
        for (const auto &component : all_components) { //
            lr::memory::ScopedStack stack;
            ImGui::PushID(static_cast<i32>(component.raw_id()));
            auto component_entity = component.entity();
            if (ImGui::MenuItem(stack.format_char("{}  {}", ICON_MDI_CUBE, component_entity.name().c_str()))) {
                selected_entity.add(component);
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    for (const auto &v : removing_components) {
        selected_entity.remove(v);
    }
    removing_components.clear();
}

InspectorWindow::InspectorWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {}

void InspectorWindow::render(this InspectorWindow &self) {
    if (ImGui::Begin(self.name.data())) {
        auto &app = EditorApp::get();
        if (app.active_project && app.active_project->active_scene_uuid && app.active_project->selected_entity) {
            draw_inspector(self);
        }
    }

    ImGui::End();
}

} // namespace led
