#include "TriangleApp.hh"

TriangleApp *pApp = nullptr;

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

    BaseApplicationDesc appDesc = {};
    appDesc.m_Name = "Hello Triangle";
    // appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "Hello Triangle",
        .m_CurrentMonitor = 1,
        .m_Width = 1280,
        .m_Height = 780,
        .m_Flags = WindowFlag::Resizable | WindowFlag::Centered,
    };

    pApp = new TriangleApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}