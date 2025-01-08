#include "EditorLayout.hh"

#include "Editor/EditorApp.hh"
#include "Editor/Panel/AssetBrowserPanel.hh"
#include "Editor/Panel/ConsolePanel.hh"
#include "Editor/Panel/InspectorPanel.hh"
#include "Editor/Panel/SceneBrowserPanel.hh"
#include "Editor/Panel/ViewportPanel.hh"

#include "Engine/OS/File.hh"

#include <imgui.h>
#include <imgui_internal.h>

namespace lr {
void EditorLayout::init(this EditorLayout &self) {
    self.setup_theme(EditorTheme::Dark);

    auto add_texture = [&self](std::string name, const fs::path &path) {
        auto &app = EditorApp::get();
        auto asset = app.asset_man.create_asset(AssetType::Texture, app.asset_man.asset_root_path(AssetType::Root) / "editor" / path);
        app.asset_man.load_texture(asset);
        self.editor_assets.emplace(name, asset->uuid);
    };

    add_texture("dir", "dir.png");
    add_texture("scene", "scene.png");
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
    auto [viewport_panel_id, viewport_panel] = self.add_panel<ViewportPanel>("Viewport", Icon::fa::eye);

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    auto dockspace_id_tmp = ImGui::GetID("EngineDockSpace");
    self.dockspace_id = dockspace_id_tmp;

    ImGui::DockBuilderRemoveNode(dockspace_id_tmp);
    ImGui::DockBuilderAddNode(dockspace_id_tmp, ImGuiDockNodeFlags_DockSpace | dock_node_flags);
    ImGui::DockBuilderSetNodeSize(dockspace_id_tmp, viewport->Size);

    auto main_dock_id = dockspace_id_tmp;
    auto up_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Up, 0.05f, nullptr, &main_dock_id);
    auto down_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Down, 0.30f, nullptr, &main_dock_id);
    auto left_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Left, 0.20f, nullptr, &main_dock_id);
    auto right_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Right, 0.25f, nullptr, &main_dock_id);

    ImGui::DockBuilderDockWindow("###up_dock", up_dock_id);
    ImGui::DockBuilderDockWindow(viewport_panel->name.data(), main_dock_id);
    ImGui::DockBuilderDockWindow(console_panel->name.data(), down_dock_id);
    ImGui::DockBuilderDockWindow(asset_browser_panel->name.data(), down_dock_id);
    ImGui::DockBuilderDockWindow(scene_browser_panel->name.data(), left_dock_id);
    ImGui::DockBuilderDockWindow(inspector_panel->name.data(), right_dock_id);
    ImGui::DockBuilderFinish(dockspace_id_tmp);
}

void EditorLayout::update(this EditorLayout &self) {
    ZoneScoped;

    auto &app = EditorApp::get();

    auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    if (app.active_project.has_value()) {
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

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene", nullptr)) {
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save Project", nullptr, false)) {
                    // app.world.save_active_project();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit")) {
                    app.shutdown();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                // if (ImGui::MenuItem("New Viewport")) {
                //     self.add_panel<ViewportPanel>("Viewport 2", Icon::fa::eye);
                // }
                //
                // ImGui::Separator();

                if (ImGui::MenuItem("Task Graph Profiler")) {
                    self.show_profiler = !self.show_profiler;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();

        if (self.show_profiler) {
            app.world_renderer.draw_profiler_ui();
        }

        for (auto &panel : self.panels) {
            panel->do_update();
        }
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize(viewport->Size);

        ImGui::Begin("New Project", nullptr, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration);

        auto &style = ImGui::GetStyle();
        auto window_size = ImGui::GetWindowSize();
        auto frame_padding = style.FramePadding;
        f32 item_width = 300.0f;
        f32 window_width = (item_width * 2.0f) + 4.0f;
        ImGui::SetCursorPos({ (window_size.x - frame_padding.x - window_width) * 0.5f, 100.0f });

        ImGui::BeginGroup();
        ImGui::TextUnformatted("Open recent project...");
        if (!app.recent_projects.empty()) {
            for (const auto &v : app.recent_projects) {
                const auto &path_str = v.string();
                if (ImGui::Button(path_str.c_str(), { item_width, 50 })) {
                    app.open_project(v / "world.lrproj");
                }
            }
        } else {
            ImGui::InvisibleButton("", { item_width, 400 });
        }
        ImGui::EndGroup();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 4.0);
        ImGui::SameLine();

        ImGui::BeginChild("##create_project_right", { item_width, 0 });
        ImGui::TextUnformatted("Create new project...");

        static std::string project_name = {};
        static std::string project_root_path = {};

        ImGui::SeparatorText("Project Name");
        ImGui::InputText("##project_name", &project_name);
        ImGui::SeparatorText("Project Path");
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

        if (ImGui::Button("OK", ImVec2(item_width, 0))) {
            app.new_project(project_root_path, project_name);
        }

        if (disabled) {
            ImGui::EndDisabled();
        }

        ImGui::EndChild();
        ImGui::End();

        ImGui::PopStyleVar(2);
    }
}
}  // namespace lr

bool ImGuiLR::drag_xy(glm::vec2 &coords) {
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

bool ImGuiLR::drag_xyz(glm::vec3 &coords) {
    bool value_changed = false;
    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

    float padding = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 button_size = { 3.0, padding };

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

void ImGuiLR::center_text(std::string_view str) {
    auto window_size = ImGui::GetWindowSize();
    auto text_size = ImGui::CalcTextSize(str.begin(), str.end());

    ImGui::SetCursorPos({ (window_size.x - text_size.x) * 0.5f, (window_size.y - text_size.y) * 0.5f });
    ImGui::TextUnformatted(str.begin(), str.end());
}

bool ImGuiLR::image_button(std::string_view text, ImTextureID texture_id, const ImVec2 &button_size) {
    auto &style = ImGui::GetStyle();
    auto min_size = std::min(button_size.x, button_size.y);
    min_size -= style.FramePadding.x * 2.0f;
    auto new_size = ImVec2(min_size, min_size);

    ImGui::BeginGroup();

    auto cursor_pos = ImGui::GetCursorPos();
    cursor_pos.x += style.FramePadding.x;
    cursor_pos.y += style.FramePadding.y;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.5, 0.9 });
    auto pressed = ImGui::Button(text.data(), button_size);
    ImGui::PopStyleVar();

    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorPos(cursor_pos);
    ImGui::Image(texture_id, new_size);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    ImGui::EndGroup();

    return pressed;
}
