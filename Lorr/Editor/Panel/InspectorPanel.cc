#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Camera.hh"
#include "Engine/World/Components.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void InspectorPanel::update(this InspectorPanel &self) {
    auto &app = EditorApp::get();
    auto active_tool = app.layout.active_tool;

    ImGui::Begin(self.name.data());
    auto query = app.ecs.query<EditorSelectedComp>();
    query.each([](flecs::entity e, EditorSelectedComp) {
        ImGui::TextUnformatted(e.name());
        if (e.has<PerspectiveCamera>()) {
            auto c = e.get_mut<PerspectiveCamera>();
            if (ImGui::CollapsingHeader("Perspective Camera")) {
                ImGui::DragFloat3("Position", &c->position[0]);
                ImGui::DragFloat("Yaw", &c->yaw);
                ImGui::DragFloat("Pitch", &c->pitch);
                ImGui::DragFloat("FOV", &c->fov);
            }
        }
    });

    switch (active_tool) {
        case ActiveTool::Cursor:
            break;
        case ActiveTool::Atmosphere: {
            ImGui::SeparatorText(LRED_ICON_SUN "  Atmosphere");
            bool update_rotation = false;
            update_rotation |= ImGui::SliderFloat2("Sun Rotation", &self.sun_dir.x, 0, 360);

            if (update_rotation) {
                auto rad = glm::radians(self.sun_dir);
                glm::vec3 direction = { glm::cos(rad.x) * glm::cos(rad.y), glm::sin(rad.y), glm::sin(rad.x) * glm::cos(rad.y) };
                app.main_render_pipeline.world_data.sun.direction = glm::normalize(direction);
            }
            break;
        }
    }

    ImGui::End();
}

}  // namespace lr
