#include "ImGui.hh"

#include "Engine.hh"

namespace lr::UI
{
    void ImGuiHandler::Init(u32 width, u32 height)
    {
        ZoneScoped;

        /// INIT IMGUI ///

        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = 0;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

        io.BackendPlatformName = "imgui_impl_lorr";
        io.DisplaySize = ImVec2((f32)width, (f32)height);

        ImGui::StyleColorsDark();

        m_Timer.start();
    }

    void ImGuiHandler::NewFrame(f32 width, f32 height)
    {
        ZoneScoped;

        ImGui::GetMainViewport();
        ImGuiIO &io = ImGui::GetIO();

        io.DisplaySize = ImVec2(width, height);
        io.DeltaTime = m_Timer.elapsed();
        m_Timer.reset();

        ImGuiMouseCursor imguiCursor = ImGui::GetMouseCursor();
        if (m_Cursor != imguiCursor)
        {
            m_Cursor = imguiCursor;
            
            EngineEventData data = { .m_WindowCursor = (WindowCursor)imguiCursor };
            Engine::Get()->PushEvent(ENGINE_EVENT_CURSOR_STATE, data);
        }

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