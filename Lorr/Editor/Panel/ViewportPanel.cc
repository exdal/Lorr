#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

namespace lr {
ViewportPanel::ViewportPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void ViewportPanel::update(this ViewportPanel &self) {
    auto &app = EditorApp::get();
    auto &scene = app.scene_at(app.active_scene.value());
    auto camera = scene.active_camera->get_mut<Component::Camera>();
    auto camera_transform = scene.active_camera->get_mut<Component::Transform>();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    auto work_area_size = ImGui::GetContentRegionAvail();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<iptr>(app.main_render_pipeline.final_image)), work_area_size);

    if (ImGui::IsWindowHovered()) {
        constexpr static f32 velocity = 3.0;
        bool reset_z = false;
        bool reset_x = false;

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            camera->velocity.z = velocity;
            reset_z |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            camera->velocity.z = -velocity;
            reset_z |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            camera->velocity.x = -velocity;
            reset_x |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            camera->velocity.x = velocity;
            reset_x |= true;
        }

        if (!reset_z) {
            camera->velocity.z = 0.0;
        }

        if (!reset_x) {
            camera->velocity.x = 0.0;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            auto drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0);
            camera_transform->rotation.x += drag.x * 0.1f;
            camera_transform->rotation.y -= drag.y * 0.1f;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    } else {
        camera->velocity = {};
    }

    ImGui::End();
}

}  // namespace lr
