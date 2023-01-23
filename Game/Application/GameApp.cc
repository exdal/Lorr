#include "GameApp.hh"

void GameApp::Init(lr::ApplicationDesc &desc)
{
    ZoneScoped;

    m_Name = desc.m_Name;

    m_Engine.Init(desc.m_EngineDesc);
}

void GameApp::Shutdown()
{
    ZoneScoped;
}

void GameApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    ImGui::Begin("Test");
    ImGui::Text("Hello");
    ImGui::End();
}
