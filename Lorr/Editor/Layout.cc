#include "Layout.hh"

#include <imgui.h>
#include <imgui_internal.h>

namespace lr {
void EditorLayout::setup_theme(this EditorLayout &self, EditorTheme theme) {
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
}

void EditorLayout::draw_base(this EditorLayout &self) {
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

    // Setup dockspace
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    auto dockspace_id = ImGui::GetID("EditorDockSpace");
    if (!ImGui::DockBuilderGetNode(dockspace_id)) {
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
        ImGui::DockBuilderDockWindow("###most_left_dock", most_left_dock_id);
        ImGui::DockBuilderDockWindow("###left_dock", left_dock_id);
        ImGui::DockBuilderDockWindow("###right_dock", right_dock_id);
        ImGui::DockBuilderDockWindow("###down_dock", down_dock_id);
        ImGui::DockBuilderDockWindow("###down_dock2", down_dock_id);
        ImGui::DockBuilderDockWindow("###center_dock", main_dock_id);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dock_node_flags | ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::PopStyleVar();

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextWindowClass(&window_class);
    ImGui::Begin("###most_left_dock");

    auto v = ImGui::GetContentRegionAvail();
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.0f);
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::Button("X", ImVec2(v.x, v.x));
    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::Begin("Viewport###center_dock");
    ImGui::End();

    ImGui::Begin("Scene Browser###left_dock");
    ImGui::End();

    ImGui::Begin("Inspector###right_dock");
    ImGui::End();

    ImGui::Begin("Asset Browser###down_dock");
    ImGui::End();

    ImGui::Begin("Console###down_dock2");
    ImGui::End();
}
}  // namespace lr
