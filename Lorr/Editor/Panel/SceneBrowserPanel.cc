#include "Panels.hh"

namespace lr {
SceneBrowserPanel::SceneBrowserPanel(std::string_view name_, c8 icon_code_, bool open_)
    : PanelI(name_, icon_code_, open_) {
}

void SceneBrowserPanel::update(this SceneBrowserPanel &self) {
    ImGui::Begin(self.name.data());
    ImGui::End();
}

}  // namespace lr
