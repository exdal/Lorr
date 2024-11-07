#include "Panels.hh"

#include "EditorApp.hh"

namespace lr {
ToolsPanel::ToolsPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void ToolsPanel::update(this ToolsPanel &self) {
    auto &app = EditorApp::get();
    auto &active_tool = app.layout.active_tool;

    ImGuiDockNodeFlags dock_node_flags_override = ImGuiDockNodeFlags_NoTabBar;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoDockingOverMe;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoDockingSplit;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoResizeX;
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = dock_node_flags_override;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.082f, 0.082f, 0.082f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.082f, 0.082f, 0.082f, 1.00f));
    ImGui::SetNextWindowClass(&window_class);
    ImGui::Begin(self.name.data());

    auto work_area_size = ImGui::GetContentRegionAvail();
    auto tool_button = [&](ActiveTool tool, const c8 *icon) {
        if (active_tool == tool) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.000f, 0.439f, 0.878f, 0.824f));
        }
        if (ImGui::Button(icon, ImVec2(work_area_size.x, work_area_size.x))) {
            active_tool = tool;
        }
        ImGui::PopStyleColor(active_tool == tool);
    };

    // Cursor

    tool_button(ActiveTool::Cursor, Icon::fa::arrow_pointer);
    ImGui::BeginDisabled();
    ImGui::Button(Icon::fa::paintbrush, ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button(Icon::fa::seedling, ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button(Icon::fa::ruler, ImVec2(work_area_size.x, work_area_size.x));
    ImGui::Button(Icon::fa::egg, ImVec2(work_area_size.x, work_area_size.x));
    ImGui::EndDisabled();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);
    tool_button(ActiveTool::World, Icon::fa::earth_americas);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

}  // namespace lr
