#include "EditorLayout.hh"

#include <imgui.h>
#include <imgui_internal.h>

namespace lr {
void EditorLayout::init(this EditorLayout &self) {
    self.setup_theme(EditorTheme::Dark);
}

void EditorLayout::setup_theme(this EditorLayout &, EditorTheme theme) {
    ZoneScoped;

#define LR_RGB(r, g, b) ImVec4((r) / 250.0f, (g) / 255.0f, (b) / 255.0f, 1.0f)

    auto &s = ImGui::GetStyle();
    auto &colors = s.Colors;
    switch (theme) {
        default:
            colors[ImGuiCol_Text] = LR_RGB(218, 218, 218);
            colors[ImGuiCol_WindowBg] = LR_RGB(32, 32, 32);
            colors[ImGuiCol_MenuBarBg] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_DockingEmptyBg] = LR_RGB(0, 0, 0);
            colors[ImGuiCol_DockingPreview] = LR_RGB(22, 22, 22);
            colors[ImGuiCol_Border] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
            colors[ImGuiCol_ResizeGripHovered] = LR_RGB(24, 24, 24);
            colors[ImGuiCol_ResizeGripActive] = LR_RGB(28, 28, 28);
            colors[ImGuiCol_Separator] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_SeparatorHovered] = LR_RGB(24, 24, 24);
            colors[ImGuiCol_SeparatorActive] = LR_RGB(28, 28, 28);
            colors[ImGuiCol_Header] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_HeaderHovered] = LR_RGB(24, 24, 24);
            colors[ImGuiCol_HeaderActive] = LR_RGB(28, 28, 28);
            colors[ImGuiCol_FrameBg] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_FrameBgHovered] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_FrameBgActive] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_Button] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_ButtonHovered] = LR_RGB(24, 24, 24);
            colors[ImGuiCol_ButtonActive] = LR_RGB(28, 28, 28);
            colors[ImGuiCol_TitleBg] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_TitleBgActive] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_TitleBgCollapsed] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_Tab] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_TabHovered] = LR_RGB(32, 32, 32);
            colors[ImGuiCol_TabActive] = LR_RGB(32, 32, 32);
            colors[ImGuiCol_TabUnfocused] = LR_RGB(15, 15, 15);
            colors[ImGuiCol_TabUnfocusedActive] = LR_RGB(32, 32, 32);
            colors[ImGuiCol_SliderGrab] = LR_RGB(32, 32, 32);
            colors[ImGuiCol_SliderGrabActive] = LR_RGB(22, 22, 22);
            break;
    }

#undef LR_RGB
}

void EditorLayout::setup_dockspace(this EditorLayout &self) {
    ZoneScoped;

    auto [asset_browser_panel_id, asset_browser_panel] = self.add_panel<AssetBrowserPanel>("Asset Browser", 0);
    auto [console_panel_id, console_panel] = self.add_panel<ConsolePanel>("Console", 0);
    auto [inspector_panel_id, inspector_panel] = self.add_panel<InspectorPanel>("Inspector", 0);
    auto [scene_browser_panel_id, scene_browser_panel] = self.add_panel<SceneBrowserPanel>("Scene Browser", 0);
    auto [tools_panel_id, tools_panel] = self.add_panel<ToolsPanel>("Tools", 0);
    auto [viewport_panel_id, viewport_panel] = self.add_panel<ViewportPanel>("Viewport", 0);

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    auto dockspace_id = ImGui::GetID("EngineDockSpace");
    self.dockspace_id = dockspace_id;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | dock_node_flags);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    auto main_dock_id = dockspace_id;
    auto up_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Up, 0.05f, nullptr, &main_dock_id);
    auto most_left_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Left, 0.03f, nullptr, &main_dock_id);
    auto down_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Down, 0.30f, nullptr, &main_dock_id);
    auto left_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Left, 0.20f, nullptr, &main_dock_id);
    auto right_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Right, 0.25f, nullptr, &main_dock_id);

    ImGui::DockBuilderDockWindow("###up_dock", up_dock_id);
    ImGui::DockBuilderDockWindow(tools_panel->name.data(), most_left_dock_id);
    ImGui::DockBuilderDockWindow(viewport_panel->name.data(), main_dock_id);
    ImGui::DockBuilderDockWindow(console_panel->name.data(), down_dock_id);
    ImGui::DockBuilderDockWindow(asset_browser_panel->name.data(), down_dock_id);
    ImGui::DockBuilderDockWindow(scene_browser_panel->name.data(), left_dock_id);
    ImGui::DockBuilderDockWindow(inspector_panel->name.data(), right_dock_id);
    ImGui::DockBuilderFinish(dockspace_id);
}

void EditorLayout::update(this EditorLayout &self) {
    ZoneScoped;

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);

    i32 dock_window_flags = ImGuiWindowFlags_NoDocking;
    dock_window_flags |= ImGuiWindowFlags_NoTitleBar;
    dock_window_flags |= ImGuiWindowFlags_NoCollapse;
    dock_window_flags |= ImGuiWindowFlags_NoResize;
    dock_window_flags |= ImGuiWindowFlags_NoMove;
    dock_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    dock_window_flags |= ImGuiWindowFlags_NoNavFocus;
    dock_window_flags |= ImGuiWindowFlags_NoScrollbar;
    dock_window_flags |= ImGuiWindowFlags_NoScrollWithMouse;
    dock_window_flags |= ImGuiWindowFlags_NoBackground;
    dock_window_flags |= ImGuiWindowFlags_MenuBar;
    ImGui::Begin("EditorDock", nullptr, dock_window_flags);
    ImGui::PopStyleVar(3);

    if (!self.dockspace_id) {
        self.setup_dockspace();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(self.dockspace_id.value(), ImVec2(0.0f, 0.0f), dock_node_flags | ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::PopStyleVar();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                // ok
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();

    ImGuiDockNodeFlags dock_node_flags_override = ImGuiDockNodeFlags_NoTabBar;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoDockingOverMe;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoDockingSplit;
    dock_node_flags_override |= ImGuiDockNodeFlags_NoResizeX;
    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = dock_node_flags_override;

    ImGui::SetNextWindowClass(&window_class);
    ImGui::Begin("###up_dock");
    ImGui::Text("Project name etc");
    ImGui::End();

    for (auto &panel : self.panels) {
        panel->do_update();
    }
}
}  // namespace lr
