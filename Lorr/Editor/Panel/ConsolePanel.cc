#include "Panels.hh"

namespace lr {
ConsolePanel::ConsolePanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void ConsolePanel::update(this ConsolePanel &self) {
    ImGui::Begin(self.name.data());
    ImGui::End();
}

}  // namespace lr
