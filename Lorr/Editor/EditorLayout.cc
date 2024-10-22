#include "EditorLayout.hh"

#include "EditorApp.hh"

#include "Engine/OS/OS.hh"

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

    colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.000f, 0.434f, 0.878f, 1.000f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.000f, 0.434f, 0.878f, 1.000f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.000f, 0.439f, 0.878f, 0.824f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.19f, 0.53f, 0.78f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 0.47f, 0.94f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 0.47f, 0.94f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.011f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.188f, 0.529f, 0.780f, 1.000f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.5f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;

    style.WindowPadding = ImVec2(4.0f, 4.0f);
    style.FramePadding = ImVec2(4.0f, 4.0f);
    style.TabMinWidthForCloseButton = 0.1f;
    style.CellPadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 3.0f);
    style.ItemInnerSpacing = ImVec2(2.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 12;
    style.ScrollbarSize = 14;
    style.GrabMinSize = 10;
}

void EditorLayout::setup_dockspace(this EditorLayout &self) {
    ZoneScoped;

    auto [asset_browser_panel_id, asset_browser_panel] = self.add_panel<AssetBrowserPanel>("Asset Browser", Icon::fa::photo_film);
    auto [console_panel_id, console_panel] = self.add_panel<ConsolePanel>("Console", Icon::fa::circle_info);
    auto [inspector_panel_id, inspector_panel] = self.add_panel<InspectorPanel>("Inspector", Icon::fa::wrench);
    auto [scene_browser_panel_id, scene_browser_panel] = self.add_panel<SceneBrowserPanel>("Scene Browser", Icon::fa::bars);
    auto [tools_panel_id, tools_panel] = self.add_panel<ToolsPanel>("Tools", Icon::fa::wrench);
    auto [viewport_panel_id, viewport_panel] = self.add_panel<ViewportPanel>("Viewport", Icon::fa::eye);

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
    auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

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
    ImGui::PopStyleVar(2);

    if (!self.dockspace_id) {
        self.setup_dockspace();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(self.dockspace_id.value(), ImVec2(0.0f, 0.0f), dock_node_flags | ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::PopStyleVar();

    bool open_create_project_popup = false;
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", nullptr)) {
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Project", nullptr, false, app.world.is_project_active())) {
                app.world.save_active_project();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("New Project...")) {
                open_create_project_popup = true;
            }

            if (ImGui::MenuItem("Open Project...")) {
                auto path = File::open_dialog("Open Project");
                if (!path.has_value()) {
                    LOG_ERROR("Could not open project file.");
                    return;
                }

                if (path->extension() != ".lrproj") {
                    LOG_ERROR("Project files must end with .lrproj.");
                    return;
                }

                app.world.unload_active_project();
                if (app.world.import_project(path.value())) {
                    for (auto &p : self.panels) {
                        p->do_project_refresh();
                    }
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit")) {
                app.shutdown();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("New Viewport")) {
                self.add_panel<ViewportPanel>("Viewport 2", Icon::fa::eye);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Task Graph Profiler")) {
                self.show_profiler = !self.show_profiler;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    if (open_create_project_popup) {
        ImGui::OpenPopup("###create_project_popup");
    }

    if (ImGui::BeginPopupModal("Create Project...###create_project_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::string project_name = {};
        static std::string project_root_path = {};

        ImGui::TextUnformatted("Project Name");
        ImGui::InputText("##project_name", &project_name);

        ImGui::TextUnformatted("Project Location");
        ImGui::InputText("##project_path", &project_root_path);
        ImGui::SameLine();
        if (ImGui::Button(Icon::fa::folder_open)) {
            auto path = File::open_dialog("New Project...", FileDialogFlag::Save | FileDialogFlag::DirOnly);
            if (path.has_value()) {
                project_root_path = path->string();
            }
        }

        ImGui::Separator();

        bool disabled = project_name.empty() || project_root_path.empty();
        if (disabled) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            app.world.export_project(project_name, project_root_path);
            ImGui::CloseCurrentPopup();
        }

        if (disabled) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();

    if (self.show_profiler) {
        app.world_render_pipeline.task_graph.draw_profiler_ui();
    }

    for (auto &panel : self.panels) {
        panel->do_update();
    }
}
}  // namespace lr

bool lg::drag_xy(glm::vec2 &coords) {
    bool value_changed = false;
    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

    float padding = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 button_size = { padding, padding };

    // X
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8f, 0.1f, 0.15f, 1.0f });
    if (ImGui::Button("X", button_size)) {
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    value_changed |= ImGui::DragFloat("##X", &coords.x, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::SameLine();

    // Y
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.2f, 0.7f, 0.2f, 1.0f });
    if (ImGui::Button("Y", button_size)) {
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    value_changed |= ImGui::DragFloat("##Y", &coords.y, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    return value_changed;
}

bool lg::drag_xyz(glm::vec3 &coords) {
    bool value_changed = false;
    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

    float padding = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 button_size = { padding, padding };

    // X
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.8f, 0.1f, 0.15f, 1.0f });
    if (ImGui::Button("X", button_size)) {
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    value_changed |= ImGui::DragFloat("##X", &coords.x, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::SameLine();

    // Y
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.2f, 0.7f, 0.2f, 1.0f });
    if (ImGui::Button("Y", button_size)) {
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    value_changed |= ImGui::DragFloat("##Y", &coords.y, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    ImGui::SameLine();

    // Z
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
    ImGui::PushStyleColor(ImGuiCol_Button, { 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.1f, 0.25f, 0.8f, 1.0f });
    if (ImGui::Button("Z", button_size)) {
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    value_changed |= ImGui::DragFloat("##Z", &coords.z, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();

    return value_changed;
}

void lg::center_text(std::string_view str) {
    auto window_size = ImGui::GetWindowSize();
    auto text_size = ImGui::CalcTextSize(str.begin(), str.end());

    ImGui::SetCursorPos({ (window_size.x - text_size.x) * 0.5f, (window_size.y - text_size.y) * 0.5f });
    ImGui::TextUnformatted(str.begin(), str.end());
}
