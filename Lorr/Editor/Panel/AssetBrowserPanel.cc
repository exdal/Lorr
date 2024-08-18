#include "Panels.hh"

namespace lr {
AssetBrowserPanel::AssetBrowserPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void AssetBrowserPanel::update(this AssetBrowserPanel &self) {
    ImGui::Begin(self.name.data());
    ImGui::End();
}

}  // namespace lr
