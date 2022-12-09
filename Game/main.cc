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

    appDesc.Window.Title = "Test";
    appDesc.Window.Width = 1280;
    appDesc.Window.Height = 780;
    appDesc.Window.Flags = WindowFlags::Centered | WindowFlags::Resizable;
    appDesc.Window.CurrentMonitor = 1;

    pApp = new GameApp;
    pApp->Init(appDesc);
    pApp->Run();

    return 0;
}