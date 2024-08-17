#include "Panels.hh"

namespace lr {
InspectorPanel::InspectorPanel(std::string_view name_, c8 icon_code_, bool open_)
    : PanelI(name_, icon_code_, open_) {
}

void InspectorPanel::update(this InspectorPanel &self) {
    ImGui::Begin(self.name.data());
    ImGui::End();
}

}  // namespace lr
