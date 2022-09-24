#include "Engine/Core/Engine.hh"

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

    pEngine = new Engine;
    pEngine->Init(appDesc, windowDesc);

    return 0;
}