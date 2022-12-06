#include "Engine.hh"

#include "Core/Utils/Timer.hh"

namespace lr
{
    void Engine::Init(ApplicationDesc &desc, WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);
        m_ImGui.Init(m_Window.m_Width, m_Window.m_Height);
        m_RendererMan.Init(Renderer::APIType::Vulkan, Graphics::APIFlags::None, &m_Window);

        Run();
    }

    void Engine::Poll(f32 deltaTime)
    {
        ZoneScoped;

        u32 eventID = LR_INVALID_EVENT_ID;
        while (m_Window.m_EventManager.Peek(eventID))
        {
            WindowEventData data = {};
            Event event = m_Window.m_EventManager.Dispatch(eventID, data);

            switch (event)
            {
                case WINDOW_EVENT_QUIT:
                {
                    m_ShuttingDown = true;
                    break;
                }

                case WINDOW_EVENT_RESIZE:
                {
                    break;
                }

                case WINDOW_EVENT_MOUSE_WHEEL:
                {
                    break;
                }

                case WINDOW_EVENT_KEYBOARD_STATE:
                {
                    LOG_TRACE("Key state: {}, {}", data.Key, (bool)data.KeyState);
                    break;
                }

                default: break;
            }
        }
    }

    void Engine::Run()
    {
        ZoneScoped;

        Timer timer;

        while (!m_ShuttingDown)
        {
            f32 deltaTime = timer.elapsed();
            timer.reset();

            m_Window.Poll();
            Poll(deltaTime);
            m_RendererMan.Poll();
        }
    }

}  // namespace lr