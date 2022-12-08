#include "ImGui.hh"

namespace lr::UI
{
    void ImGuiHandler::Init(u32 width, u32 height)
    {
        ZoneScoped;

        /// INIT IMGUI ///

        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = 0;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;  // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;   // We can honor io.WantSetMousePos requests (optional, rarely used)

        io.BackendPlatformName = "imgui_impl_lorr";
        io.DisplaySize = ImVec2((float)width, (float)height);

        ImGui::StyleColorsDark();
    }

    void ImGuiHandler::NewFrame(float width, float height)
    {
        ZoneScoped;

        ImGui::GetMainViewport();
        ImGuiIO &io = ImGui::GetIO();

        io.DisplaySize = ImVec2(width, height);

        ImGui::NewFrame();
    }

    void ImGuiHandler::EndFrame()
    {
        ZoneScoped;

        ImGui::EndFrame();
        ImGui::Render();
    }

    void ImGuiHandler::Destroy()
    {
        ZoneScoped;
    }

}  // namespace lr::UI