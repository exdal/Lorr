#include "Editor/Panel/InspectorPanel.hh"

#include "Editor/EditorApp.hh"

#include "Engine/Scene/ECSModule/ComponentWrapper.hh"
#include "Engine/Scene/ECSModule/Core.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void InspectorPanel::render(this InspectorPanel &self) {
    auto &app = EditorApp::get();

    ImGui::Begin(self.name.data());
    if (app.active_scene_uuid.has_value()) {
        self.draw_inspector();
    }

    ImGui::End();
}

auto InspectorPanel::draw_inspector(this InspectorPanel &) -> void {
    auto &app = EditorApp::get();
    auto *scene = app.asset_man.get_scene(app.active_scene_uuid.value());
    auto region = ImGui::GetContentRegionAvail();

    if (app.selected_entity) {
        if (ImGui::Button(app.selected_entity.name().c_str(), ImVec2(region.x, 0))) {
            // TODO: Rename entity
        }

        bool entity_modified = false;
        app.selected_entity.each([&](flecs::id component_id) {
            memory::ScopedStack stack;

            const auto &ecs_world = app.selected_entity.world();
            if (!component_id.is_entity()) {
                return;
            }

            ECS::ComponentWrapper component(app.selected_entity, component_id);
            if (!component.has_component()) {
                return;
            }

            auto name_with_icon = stack.format_char("{}", component.name);
            if (ImGui::CollapsingHeader(name_with_icon, nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
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
                    ImGui::TextUnformatted(member_name.data(), member_name.data() + member_name.length());
                    ImGui::TableNextColumn();

                    ImGui::PushID(static_cast<i32>(i));
                    std::visit(
                        match{
                            [](const auto &) {},
                            [&](f32 *v) { entity_modified |= ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_Float); },
                            [&](i32 *v) { entity_modified |= ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_S32); },
                            [&](u32 *v) { entity_modified |= ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_U32); },
                            [&](i64 *v) { entity_modified |= ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_S64); },
                            [&](u64 *v) { entity_modified |= ImGuiLR::drag_vec(0, v, 1, ImGuiDataType_U64); },
                            [&](glm::vec2 *v) { entity_modified |= ImGuiLR::drag_vec(0, glm::value_ptr(*v), 2, ImGuiDataType_Float); },
                            [&](glm::vec3 *v) { entity_modified |= ImGuiLR::drag_vec(0, glm::value_ptr(*v), 3, ImGuiDataType_Float); },
                            [](std::string *v) { ImGui::InputText("", v); },
                            [&](UUID *v) {
                                auto cursor_pos = ImGui::GetCursorPos();
                                auto avail_region = ImGui::GetContentRegionAvail();
                                auto *asset = app.asset_man.get_asset(*v);
                                if (!asset) {
                                    ImGui::TextUnformatted("Drop a model here.");
                                    *v = {};
                                } else {
                                    const auto &model_name = asset->path.filename();
                                    ImGuiLR::text_sv(stack.format("Name: {}", model_name));
                                    ImGuiLR::text_sv(stack.format("ModelID: {}", std::to_underlying(asset->model_id)));
                                    ImGuiLR::text_sv(stack.format("UUID: {}", v->str()));
                                }

                                ImGui::SetCursorPos(cursor_pos);
                                ImGui::InvisibleButton(reinterpret_cast<const c8 *>(&*v), { avail_region.x, 50.0f });
                                if (ImGui::BeginDragDropTarget()) {
                                    if (const auto *asset_payload = ImGui::AcceptDragDropPayload("ASSET_BY_UUID")) {
                                        auto &old_uuid = *v;
                                        auto &dropping_uuid = *static_cast<UUID *>(asset_payload->Data);
                                        auto *dropping_asset = app.asset_man.get_asset(dropping_uuid);
                                        if (dropping_asset->type == AssetType::Model) {
                                            if (old_uuid != dropping_uuid && app.asset_man.load_model(dropping_uuid) && old_uuid) {
                                                app.asset_man.unload_model(old_uuid);
                                            }
                                            old_uuid = dropping_uuid;
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }
                            },
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

            auto &entity_db = scene->get_entity_db();
            auto all_components = entity_db.get_components();
            for (const auto &component : all_components) {  //
                ImGui::PushID(static_cast<i32>(component.raw_id()));
                auto component_entity = component.entity();
                if (ImGui::MenuItem(stack.format_char("{}  {}", Icon::fa::cube, component_entity.name().c_str()))) {
                    app.selected_entity.add(component);
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }

        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

        if (auto gpu_entity_id = scene->get_gpu_entity(app.selected_entity); entity_modified) {
            scene->set_dirty(gpu_entity_id.value());
        }
    }
}

}  // namespace lr
