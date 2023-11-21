// Created on Friday December 9th 2022 by exdal
// Last modified on Sunday June 25th 2023 by exdal
#include "Application.hh"
#include <stdarg.h>

#include "Core/BackTrace.hh"
#include "OS/Directory.hh"
#include "Utils/Timer.hh"

namespace lr
{
void Application::create(BaseApplicationDesc &desc)
{
    ZoneScoped;
    
    eastl::string currentPath;
    OS::GetCurrentDir(currentPath);
    currentPath += "\\bin";
    OS::SetLibraryDirectory(currentPath);

#if !_DEBUG
    BackTrace::Init();
#endif 

    m_name = desc.m_name;
    m_engine.create(desc.m_engine_desc);
}

void Application::run()
{
    ZoneScoped;

    m_engine.prepare();

    Timer timer;
    while (!m_engine.m_shutting_down)
    {
        f32 deltaTime = timer.elapsed();
        timer.reset();

        m_engine.begin_frame();
        poll(deltaTime);
        m_engine.end_frame();

        FrameMark;
    }
}

}  // namespace lr