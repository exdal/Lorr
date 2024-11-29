#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void InspectorPanel::update(this InspectorPanel &self) {
    auto &app = EditorApp::get();
    auto &world = app.world;

    ImGui::Begin(self.name.data());
    auto region = ImGui::GetContentRegionAvail();

    auto query = world.ecs().query<Component::EditorSelected>();
    query.each([&](flecs::entity e, Component::EditorSelected) {
        if (ImGui::Button(e.name().c_str(), ImVec2(region.x, 0))) {
            // TODO: Rename entity
        }

        e.each([&](flecs::id component_id) {
            auto ecs_world = e.world();
            if (!component_id.is_entity()) {
                return;
            }

            Component::Wrapper component(e, component_id);
            if (!component.has_component()) {
                return;
            }

            auto name_with_icon = std::format("{}  {}", world.component_icon(component_id.raw_id()), component.name);
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
                            [](glm::vec2 *v) { lg::drag_xy(*v); },
                            [](glm::vec3 *v) { lg::drag_xyz(*v); },
                            [](std::string *v) { ImGui::InputText("", v); },
                            [](Identifier *v) { ImGui::TextUnformatted(v->sv().data()); },
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

    switch (app.layout.active_tool) {
        case ActiveTool::World: {
            auto &world_data = app.world_renderer.world_data();
            auto &pbr_flags = app.world_renderer.get_pbr_flags();
            auto name_with_icon = std::format("{}  World", Icon::fa::earth_americas);
            ImGui::SeparatorText(name_with_icon.c_str());

            if (ImGui::CollapsingHeader("Sun", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &sun = world_data.sun;
                bool update_sun = ImGui::DragFloat2("Sun Direction", glm::value_ptr(app.world_renderer.sun_angle()));
                ImGui::DragFloat("Intensity", &sun.intensity);

                if (update_sun) {
                    app.world_renderer.update_sun_dir();
                }
            }

            if (ImGui::CollapsingHeader("Atmosphere", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &atmos = world_data.atmosphere;
                if (ImGui::Checkbox("Atmosphere##Enable", &pbr_flags.render_sky)) {
                    app.world_renderer.update_pbr_graph();
                }

                ImGui::DragFloat3("Rayleigh Scattering", glm::value_ptr(atmos.rayleigh_scatter), 0.00001, 0.0, 0.01);
                ImGui::DragFloat("Rayleigh Density", &atmos.rayleigh_density, 0.1);
                ImGui::DragFloat3("Mie Scattering", glm::value_ptr(atmos.mie_scatter), 0.00001, 0.0, 0.01);
                ImGui::DragFloat("Mie Density", &atmos.mie_density, 0.1);
                ImGui::DragFloat("Mie Extinction", &atmos.mie_extinction, 0.00001);
                ImGui::DragFloat("Mie Asymmetry", &atmos.mie_asymmetry, 0.1);
                ImGui::DragFloat3("Ozone Absorption", glm::value_ptr(atmos.ozone_absorption), 0.00001, 0.0, 0.01);
                ImGui::DragFloat("Ozone Height", &atmos.ozone_height);
                ImGui::DragFloat("Ozone Thickness", &atmos.ozone_thickness);
                ImGui::DragFloat3("Terrain Albedo", glm::value_ptr(atmos.terrain_albedo), 0.01, 0.0, 1.0);
                ImGui::DragFloat("Aerial KM per slice", &atmos.aerial_km_per_slice);
                ImGui::DragFloat("Planet Radius", &atmos.planet_radius);
                ImGui::DragFloat("Atmosphere Radius", &atmos.atmos_radius);
            }

            if (ImGui::CollapsingHeader("Clouds", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &clouds = world_data.clouds;
                if (ImGui::Checkbox("Clouds##Enable", &pbr_flags.render_clouds)) {
                    app.world_renderer.update_pbr_graph();
                }

                ImGui::DragFloat2("Layer Bounds", glm::value_ptr(clouds.bounds));
                ImGui::DragFloat("Global Scale", &clouds.global_scale, 0.0001, 0.00001, 1.0);
                ImGui::DragFloat("Shape Noise Scale", &clouds.shape_noise_scale, 0.0001, 0.00001, 1.0);
                ImGui::DragFloat("Detail Noise Scale", &clouds.detail_noise_scale, 0.0001, 0.00001, 1.0);
                ImGui::DragFloat("Coverage", &clouds.coverage, 0.001, 0.0, 1.0);
                ImGui::DragFloat("Detail Noise Influence", &clouds.detail_noise_influence, 0.0001, 0.00001, 1.0);
                ImGui::DragFloat("General Density", &clouds.general_density, 0.0001, 0.00001, 1.0);
                ImGui::SliderFloat("Cloud Type", &clouds.cloud_type, 0.0, 1.0);

                ImGui::SliderFloat3("Cloud Shape Weights", glm::value_ptr(clouds.shape_noise_weights), 0.0, 1.0);
                ImGui::SliderFloat3("Cloud Detail Weights", glm::value_ptr(clouds.detail_noise_weights), 0.0, 1.0);
                ImGui::SliderFloat("Cloud Darkness Threshold", &clouds.darkness_threshold, 0.0, 1.0);
                ImGui::SliderFloat3("Cloud Phase Consts.", glm::value_ptr(clouds.phase_values), -1.0, 1.0);

                ImGui::DragInt("Sun Step Count", &clouds.sun_step_count, 1, 0, 16);
                ImGui::DragInt("Clouds Step Count", &clouds.clouds_step_count, 1, 0, 64);

                ImGui::DragFloat("Self Light Absorption", &clouds.cloud_light_absorption, 0.0001, 0.00001, 1.0);
                ImGui::DragFloat("Sun Light Absorption", &clouds.sun_light_absorption, 0.0001, 0.00001, 1.0);
            }

            break;
        }
        case ActiveTool::Cursor:
        case ActiveTool::TerrainBrush:
        case ActiveTool::DecalBrush:
        case ActiveTool::MaterialEditor:
            break;
    }

    ImGui::End();
}

}  // namespace lr
