#include "Panels.hh"

namespace lr {
ToolsPanel::ToolsPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
}

void ToolsPanel::update(this ToolsPanel &self) {
    ImGuiDockNodeFlags dock_node_flags_override = ImGuiDockNodeFlags_NoTabBar;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoDockingOverMe;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoDockingSplit;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoResizeX;
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = dock_node_flags_override;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextWindowClass(&window_class);
    ImGui::Begin(self.name.data());

    auto work_area_size = ImGui::GetContentRegionAvail();
    ImGui::Button("\uf245", ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button("\uf1fc", ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button("\uf4d8", ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button("\uf545", ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button("\uf7fb", ImVec2(work_area_size.x, work_area_size.x));
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);
    ImGui::Button("\uf57d", ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button("\uf185", ImVec2(work_area_size.x, work_area_size.x));

    ImGui::End();
    ImGui::PopStyleVar(2);
}

}  // namespace lr
