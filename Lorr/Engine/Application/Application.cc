// Created on Friday December 9th 2022 by exdal
// Last modified on Sunday May 21st 2023 by exdal
#include "Application.hh"

#include "Core/BackTrace.hh"
#include "Utils/Timer.hh"

namespace lr
{
void Application::PreInit(BaseApplicationDesc &desc)
{
    ZoneScoped;

    m_Name = desc.m_Name;
    m_Engine.Init(desc.m_EngineDesc);

    m_Engine.m_RenderGraph.Init(nullptr);
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
    }
}

}  // namespace lr