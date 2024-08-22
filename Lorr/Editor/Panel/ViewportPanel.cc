#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Camera.hh"

namespace lr {
ViewportPanel::ViewportPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void ViewportPanel::update(this ViewportPanel &self) {
    auto &app = EditorApp::get();
    auto &scene = app.scene_at(app.active_scene.value());
    auto camera = scene.active_camera->get_mut<PerspectiveCamera>();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    auto work_area_size = ImGui::GetContentRegionAvail();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<iptr>(app.main_render_pipeline.final_image)), work_area_size);
    ImGui::SetCursorPos(ImVec2(5, 30));

    if (ImGui::IsWindowHovered()) {
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            camera->velocity.z = 1.0;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            camera->velocity.x = -1.0;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            camera->velocity.z = -1.0;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            camera->velocity.x = 1.0;
        }
    }
    ImGui::End();
}

}  // namespace lr
