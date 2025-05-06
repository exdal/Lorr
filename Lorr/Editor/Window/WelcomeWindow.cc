#include "Editor/Window/WelcomeWindow.hh"

namespace led {
WelcomeWindow::WelcomeWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {}

auto WelcomeWindow::render(this WelcomeWindow &self, vuk::Format, vuk::Extent3D) -> void {
    ZoneScoped;

    if (ImGui::Begin(self.name.c_str())) {
    }

    ImGui::End();
}

} // namespace led
