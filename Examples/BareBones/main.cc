// Created on Wednesday May 17th 2023 by exdal
// Last modified on Sunday July 16th 2023 by exdal

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
    // appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "BareBones",
        .m_CurrentMonitor = 1,
        .m_Width = 1280,
        .m_Height = 780,
        .m_Flags = WindowFlag::Resizable | WindowFlag::Centered,
    };

    pApp = new LorrApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}