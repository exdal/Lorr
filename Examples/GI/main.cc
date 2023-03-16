#include "GIApp.hh"

GIApp *pApp = nullptr;

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
    appDesc.m_Name = "GI Demo";
    appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "how is this working",
        .m_CurrentMonitor = 0,
        .m_Width = 1280,
        .m_Height = 780,
        .m_Flags = LR_WINDOW_FLAG_RESIZABLE | LR_WINDOW_FLAG_CENTERED,
    };

    pApp = new GIApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}