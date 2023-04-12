#include "ImguiApp.hh"

void ImguiApp::Init(lr::BaseApplicationDesc &desc)
{
    ZoneScoped;

    PreInit(desc);
}

void ImguiApp::Shutdown()
{
    ZoneScoped;
}

void ImguiApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    ImGuiIO &io = ImGui::GetIO();
    static bool showDemo = true;

    ImGui::Begin(m_Name.data());
    ImGui::Checkbox("Show Demo", &showDemo);
    ImGui::Text("FPS: %f(%f)", io.Framerate, io.DeltaTime);
    ImGui::End();

    if (showDemo)
        ImGui::ShowDemoWindow(&showDemo);
}
