// Created on Thursday January 26th 2023 by exdal
// Last modified on Sunday July 16th 2023 by exdal

#include "ImguiApp.hh"

ImguiApp *pApp = nullptr;

lr::Application *lr::Application::Get()
{
    return pApp;
}

lr::Engine *lr::Engine::Get()
{
    return &pApp->Get()->m_Engine;
}

int main()
{
    using namespace lr;
    using namespace lr::Graphics;

    BaseApplicationDesc appDesc = {};
    appDesc.m_Name = "ImGui Demo";
    // appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "ImGui Demo",
        .m_CurrentMonitor = 1,
        .m_Width = 1280,
        .m_Height = 780,
        .m_Flags = WindowFlag::Resizable | WindowFlag::Centered,
    };

    pApp = new ImguiApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}