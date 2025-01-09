#include "Editor/Panel/InspectorPanel.hh"

#include "Editor/EditorApp.hh"

#include "Engine/World/ECSModule/ComponentWrapper.hh"
#include "Engine/World/ECSModule/Core.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void InspectorPanel::update(this InspectorPanel &self) {
    auto &app = EditorApp::get();

    ImGui::Begin(self.name.data());
    if (app.active_scene_id.has_value()) {
        self.draw_inspector();
    }

    ImGui::End();
}

auto InspectorPanel::draw_inspector(this InspectorPanel &) -> void {
    auto &app = EditorApp::get();
    auto *scene = app.asset_man.get_scene(app.active_scene_id.value());
    auto region = ImGui::GetContentRegionAvail();

    auto query = scene->get_world().query<ECS::EditorSelected>();
    query.each([&](flecs::entity e, ECS::EditorSelected) {
        if (ImGui::Button(e.name().c_str(), ImVec2(region.x, 0))) {
            // TODO: Rename entity
        }

        e.each([&](flecs::id component_id) {
            auto ecs_world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            ECS::ComponentWrapper component(e, component_id);
            if (!component.has_component()) {
                return;
            }

            // auto name_with_icon = std::format("{}  {}", world.component_icon(component_id.raw_id()), component.name);
            if (ImGui::CollapsingHeader(component.name.cbegin(), nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PushID(static_cast<i32>(component_id));
                ImGui::BeginTable(
                    "entity_props",
                    2,
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH
                        | ImGuiTableFlags_NoSavedSettings);
                ImGui::TableSetupColumn("label", 0, 0.5f);
                ImGui::TableSetupColumn("property", ImGuiTableColumnFlags_WidthStretch);

                ImGui::PushID(component.members_data);
                component.for_each([&](usize &i, std::string_view member_name, ECS::ComponentWrapper::Member &member) {
                    // Draw prop label
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
                    ImGui::TextUnformatted(member_name.cbegin());
                    ImGui::TableNextColumn();

                    ImGui::PushID(static_cast<i32>(i));
                    std::visit(
                        match{
                            [](const auto &) {},
                            [&](f32 *v) { ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_Float); },
                            [&](i32 *v) { ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_S32); },
                            [&](u32 *v) { ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_U32); },
                            [&](i64 *v) { ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_S64); },
                            [&](u64 *v) { ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_U64); },
                            [&](glm::vec2 *v) { ImGuiLR::drag_vec(0, glm::value_ptr(*v), 2, ImGuiDataType_Float); },
                            [&](glm::vec3 *v) { ImGuiLR::drag_vec(0, glm::value_ptr(*v), 3, ImGuiDataType_Float); },
                            [](std::string *v) { ImGui::InputText("", v); },
                            [](UUID *v) { ImGui::TextUnformatted(v->str().c_str()); },
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
            ImGui::OpenPopup("add_component");
        }

        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
        ImGui::SetNextWindowSize({ region.x, 0 });
        if (ImGui::BeginPopup("add_component")) {
            memory::ScopedStack stack;

            auto imported_modules = scene->get_imported_modules();

            auto q = scene->get_world().query_builder<flecs::Component>();
            for (const auto &mod : imported_modules) {
                q.with(flecs::ChildOf, mod);
            }

            q.with(flecs::ChildOf, flecs::Flecs).oper(flecs::Not).self().build();
            q.each([&stack, e](flecs::entity component, flecs::Component &) {  //
                ImGui::PushID(static_cast<i32>(component.raw_id()));
                if (ImGui::MenuItem(stack.format("{}  {}", Icon::fa::cube, component.name()).cbegin())) {
                    e.add(component);
                }
                ImGui::PopID();
            });
            ImGui::EndPopup();
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    });
}

}  // namespace lr
