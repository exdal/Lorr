// Created on Friday December 9th 2022 by exdal
// Last modified on Monday June 12th 2023 by exdal
#include "Application.hh"
#include <stdarg.h>

#include "Core/BackTrace.hh"
#include "OS/Directory.hh"
#include "Utils/Timer.hh"

namespace lr
{
void Application::PreInit(BaseApplicationDesc &desc)
{
    ZoneScoped;
    
    eastl::string currentPath;
    OS::GetCurrentDir(currentPath);
    currentPath += "\\bin";
    OS::SetLibraryDirectory(currentPath);

#if 1
    BackTrace::Init();
#endif 

    m_Name = desc.m_Name;
    m_Engine.Init(desc.m_EngineDesc);
}

void Application::Run()
{
    ZoneScoped;

    m_Engine.Prepare();

    Timer timer;
    while (!m_Engine.m_ShuttingDown)
    {
        f32 deltaTime = timer.elapsed();
        timer.reset();

        m_Engine.BeginFrame();
        Poll(deltaTime);
        m_Engine.EndFrame();

        FrameMark;
    }
}

}  // namespace lr