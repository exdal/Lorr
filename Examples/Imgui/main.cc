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

    BaseApplicationDesc appDesc = {};
    appDesc.m_Name = "ImGui Demo";
    appDesc.m_EngineDesc.m_TargetAPI = LR_API_TYPE_VULKAN;
    appDesc.m_EngineDesc.m_TargetAPIFlags = LR_API_FLAG_NONE;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "ImGui Demo",
        .m_CurrentMonitor = 0,
        .m_Width = 1280,
        .m_Height = 780,
        .m_Flags = LR_WINDOW_FLAG_RESIZABLE | LR_WINDOW_FLAG_CENTERED,
    };

    pApp = new ImguiApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}