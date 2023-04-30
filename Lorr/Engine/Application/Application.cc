#include "Application.hh"

#include "Utils/Timer.hh"

namespace lr
{
    void Application::PreInit(BaseApplicationDesc &desc)
    {
        ZoneScoped;

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
        }
    }

}  // namespace lr