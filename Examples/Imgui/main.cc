// Created on Thursday January 26th 2023 by exdal
// Last modified on Sunday July 16th 2023 by exdal

#include "ImguiApp.hh"

ImguiApp *pApp = nullptr;

lr::Application *lr::Application::get()
{
    return pApp;
}

lr::Engine *lr::Engine::get()
{
    return &pApp->get()->m_engine;
}

int main()
{
    using namespace lr;
    BaseApplicationDesc appDesc = {};
    appDesc.m_name = "ImGui Demo";
    // appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_engine_desc.m_window_desc = {
        .m_title = "ImGui Demo",
        .m_current_monitor = 1,
        .m_width = 1280,
        .m_height = 780,
        .m_Flags = WindowFlag::Resizable | WindowFlag::Centered,
    };

    pApp = new ImguiApp;
    pApp->Init(appDesc);
    pApp->run();

    return 0;
}