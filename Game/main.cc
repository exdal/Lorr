#include "Engine.hh"

lr::Engine *pEngine = nullptr;

lr::Engine *lr::GetEngine()
{
    return pEngine;
}

int main()
{
    using namespace lr;

    ApplicationDesc appDesc;

    WindowDesc windowDesc;
    windowDesc.Title = "Test";
    windowDesc.Width = 1280;
    windowDesc.Height = 780;
    windowDesc.Flags = WindowFlags::Centered | WindowFlags::Resizable;
    windowDesc.CurrentMonitor = 1;

    pEngine = new Engine;
    pEngine->Init(appDesc, windowDesc);

    return 0;
}