#include "Panels.hh"

#include "EditorApp.hh"

namespace lr {
ViewportPanel::ViewportPanel(std::string_view name_, c8 icon_code_, bool open_)
    : PanelI(name_, icon_code_, open_) {
}

void ViewportPanel::update(this ViewportPanel &self) {
    auto &app = EditorApp::get();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    auto work_area_size = ImGui::GetContentRegionAvail();
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<iptr>(app.game_view_image_id)), work_area_size);
    ImGui::End();
}

}  // namespace lr
