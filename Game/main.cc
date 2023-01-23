#include "Application/GameApp.hh"

lr::Application *pApp = nullptr;

lr::Application *lr::GetApp()
{
    return pApp;
}

int main()
{
    using namespace lr;

    ApplicationDesc appDesc = {};
    appDesc.m_Name = "Game";
    appDesc.m_EngineDesc.m_TargetAPI = Renderer::APIType::Vulkan;
    appDesc.m_EngineDesc.m_TargetAPIFlags = Graphics::APIFlags::None;

    appDesc.m_EngineDesc.m_WindowDesc = {
        .m_Title = "Game",
        .m_CurrentMonitor = 0,
        .m_Width = 1480,
        .m_Height = 780,
        .m_Flags = WindowFlags::Centered | WindowFlags::Resizable,
    };

    pApp = new GameApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}