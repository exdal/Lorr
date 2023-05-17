// Created on Wednesday May 17th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#include "LorrApp.hh"

LorrApp *pApp = nullptr;

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
    appDesc.m_Name = "BareBones";
    appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "BareBones",
        .m_CurrentMonitor = 1,
        .m_Width = 1280,
        .m_Height = 780,
        .m_Flags = LR_WINDOW_FLAG_RESIZABLE | LR_WINDOW_FLAG_CENTERED,
    };

    pApp = new LorrApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}