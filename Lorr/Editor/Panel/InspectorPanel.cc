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

    // Temp, remove it later
    static glm::vec2 sun_rotation = {};
    bool updateRot = false;
    updateRot |= ImGui::SliderFloat("Sun Rotation X", &sun_rotation.x, 0, 360);
    updateRot |= ImGui::SliderFloat("Sun Rotation Y", &sun_rotation.y, -20, 90);

    if (updateRot) {
        auto rad = glm::radians(sun_rotation);
        glm::vec3 direction = { glm::cos(rad.x) * glm::cos(rad.y), glm::sin(rad.y), glm::sin(rad.x) * glm::cos(rad.y) };
        app.main_render_pipeline.world_data.sun.direction = glm::normalize(direction);
    }

    ImGui::End();
}

}  // namespace lr
