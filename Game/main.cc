#include "Application/GameApp.hh"

lr::Application *pApp = nullptr;

lr::Application *lr::GetApp()
{
    return pApp;
}

int main()
{
    using namespace lr;

    ApplicationDesc appDesc;
    appDesc.Title = "Game";
    appDesc.Engine.TargetAPI = lr::Renderer::APIType::Vulkan;
    appDesc.Engine.TargetAPIFlags = lr::Graphics::APIFlags::None;

    appDesc.Engine.Window.Title = "Game";
    appDesc.Engine.Window.Width = 1480;
    appDesc.Engine.Window.Height = 780;
    appDesc.Engine.Window.Flags = WindowFlags::Centered | WindowFlags::Resizable;
    appDesc.Engine.Window.CurrentMonitor = 0;

    pApp = new GameApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}