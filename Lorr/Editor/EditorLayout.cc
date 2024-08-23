#include "EditorLayout.hh"

#include "EditorApp.hh"

#include <imgui.h>
#include <imgui_internal.h>

namespace lr {
void EditorLayout::init(this EditorLayout &self) {
    self.setup_theme(EditorTheme::Dark);
}

void EditorLayout::setup_theme(this EditorLayout &, EditorTheme) {
    ZoneScoped;

    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.90f);
    colors[ImGuiCol_Border] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.80f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.30f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.15f, 0.60f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.40f, 0.40f, 0.40f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.50f, 0.50f, 0.50f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.60f, 0.60f, 0.60f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.20f, 0.20f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.50f, 0.50f, 0.50f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.05f, 1.0f, 0.05f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 0.05f, 0.05f, 0.0f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.30f, 0.30f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.90f, 0.90f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;
}

void EditorLayout::setup_dockspace(this EditorLayout &self) {
    ZoneScoped;

    auto [asset_browser_panel_id, asset_browser_panel] = self.add_panel<AssetBrowserPanel>(LRED_ICON_ASSETS "  Asset Browser");
    auto [console_panel_id, console_panel] = self.add_panel<ConsolePanel>(LRED_ICON_INFO "  Console");
    auto [inspector_panel_id, inspector_panel] = self.add_panel<InspectorPanel>(LRED_ICON_WRENCH "  Inspector");
    auto [scene_browser_panel_id, scene_browser_panel] = self.add_panel<SceneBrowserPanel>(LRED_ICON_SANDWICH "  Scene Browser");
    auto [tools_panel_id, tools_panel] = self.add_panel<ToolsPanel>("Tools");
    auto [viewport_panel_id, viewport_panel] = self.add_panel<ViewportPanel>(LRED_ICON_EYE "  Viewport");

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

    auto &app = EditorApp::get();
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
                app.shutdown(false);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Task Graph Profiler")) {
                self.show_profiler = !self.show_profiler;
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
    ImGui::End();

    if (self.show_profiler) {
        app.main_render_pipeline.task_graph.draw_profiler_ui();
    }

    for (auto &panel : self.panels) {
        panel->do_update();
    }
}
}  // namespace lr
