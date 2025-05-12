#include "Editor/Themes.hh"

#include <imgui.h>

namespace led {
auto Theme::dark() -> void {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;

    const ImVec4 text_color = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    const ImVec4 text_disabled = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    const ImVec4 bg_dark = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    const ImVec4 bg_mid = ImVec4(0.1f, 0.1f, 0.1f, 1.00f);
    const ImVec4 bg_light = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    const ImVec4 bg_active = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    const ImVec4 highlight = ImVec4(0.00f, 0.44f, 0.88f, 1.00f);
    const ImVec4 highlight_hover = ImVec4(0.00f, 0.47f, 0.94f, 1.00f);
    const ImVec4 border = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    const ImVec4 grab = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    const ImVec4 grab_hover = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    const ImVec4 dim_overlay = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    // Text
    colors[ImGuiCol_Text] = text_color;
    colors[ImGuiCol_TextDisabled] = text_disabled;

    // Backgrounds
    colors[ImGuiCol_WindowBg] = bg_mid;
    colors[ImGuiCol_ChildBg] = bg_mid;
    colors[ImGuiCol_PopupBg] = bg_mid;
    colors[ImGuiCol_FrameBg] = bg_light;
    colors[ImGuiCol_FrameBgHovered] = grab_hover;
    colors[ImGuiCol_FrameBgActive] = bg_active;
    colors[ImGuiCol_TitleBg] = bg_light;
    colors[ImGuiCol_TitleBgActive] = bg_light;
    colors[ImGuiCol_TitleBgCollapsed] = bg_light;
    colors[ImGuiCol_MenuBarBg] = bg_mid;

    // Borders
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = bg_dark;

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = bg_dark;
    colors[ImGuiCol_ScrollbarGrab] = grab;
    colors[ImGuiCol_ScrollbarGrabHovered] = grab_hover;
    colors[ImGuiCol_ScrollbarGrabActive] = bg_active;

    // Widgets
    colors[ImGuiCol_CheckMark] = highlight;
    colors[ImGuiCol_SliderGrab] = highlight;
    colors[ImGuiCol_SliderGrabActive] = highlight;
    colors[ImGuiCol_Button] = bg_light;
    colors[ImGuiCol_ButtonHovered] = grab_hover;
    colors[ImGuiCol_ButtonActive] = highlight;

    // Headers
    colors[ImGuiCol_Header] = bg_mid;
    colors[ImGuiCol_HeaderHovered] = grab_hover;
    colors[ImGuiCol_HeaderActive] = grab_hover;

    // Separators
    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = grab_hover;
    colors[ImGuiCol_SeparatorActive] = grab_hover;

    // Resize grips
    colors[ImGuiCol_ResizeGrip] = border;
    colors[ImGuiCol_ResizeGripHovered] = grab_hover;
    colors[ImGuiCol_ResizeGripActive] = grab_hover;

    // Tabs
    colors[ImGuiCol_Tab] = bg_dark;
    colors[ImGuiCol_TabHovered] = grab_hover;
    colors[ImGuiCol_TabActive] = bg_light;
    colors[ImGuiCol_TabUnfocused] = bg_dark;
    colors[ImGuiCol_TabUnfocusedActive] = bg_mid;

    // Docking
    colors[ImGuiCol_DockingPreview] = ImVec4(highlight.x, highlight.y, highlight.z, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    // Plots
    colors[ImGuiCol_PlotLines] = highlight;
    colors[ImGuiCol_PlotLinesHovered] = highlight_hover;
    colors[ImGuiCol_PlotHistogram] = highlight;
    colors[ImGuiCol_PlotHistogramHovered] = highlight_hover;

    // Tables
    colors[ImGuiCol_TableHeaderBg] = bg_mid;
    colors[ImGuiCol_TableBorderStrong] = border;
    colors[ImGuiCol_TableBorderLight] = border;
    colors[ImGuiCol_TableRowBg] = ImVec4(1.0f, 1.0f, 1.0f, 0);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.01f);

    // Misc
    colors[ImGuiCol_TextSelectedBg] = highlight_hover;
    colors[ImGuiCol_DragDropTarget] = highlight;
    colors[ImGuiCol_NavHighlight] = highlight;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = dim_overlay;

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
} // namespace led
