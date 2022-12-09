#include "Application.hh"

#include "Core/Utils/Timer.hh"

namespace lr
{
    void Application::Run()
    {
        ZoneScoped;

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