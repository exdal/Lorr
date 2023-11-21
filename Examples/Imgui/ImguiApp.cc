// Created on Thursday January 26th 2023 by exdal
// Last modified on Friday June 2nd 2023 by exdal

#include "ImguiApp.hh"

void ImguiApp::Init(lr::BaseApplicationDesc &desc)
{
    ZoneScoped;

    create(desc);
}

void ImguiApp::shutdown()
{
    ZoneScoped;
}

void ImguiApp::poll(f32 deltaTime)
{
    ZoneScoped;

    ImGuiIO &io = ImGui::GetIO();
    static bool showDemo = true;

    ImGui::Begin(m_name.data());
    ImGui::Checkbox("Show Demo", &showDemo);
    ImGui::Text("FPS: %f(%f)", io.Framerate, io.DeltaTime);
    ImGui::End();

    if (showDemo)
        ImGui::ShowDemoWindow(&showDemo);
}
