#include "Engine.hh"

#include "Core/Utils/Timer.hh"

namespace lr
{
    void Engine::Init(EngineDesc &engineDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(engineDesc.m_WindowDesc);
        m_ImGui.Init(m_Window.m_Width, m_Window.m_Height);
        m_RendererMan.Init(engineDesc.m_TargetAPI, engineDesc.m_TargetAPIFlags, &m_Window);
    }

    void Engine::DispatchEvents()
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
                    m_Window.m_Width = data.m_SizeWidth;
                    m_Window.m_Height = data.m_SizeHeight;

                    m_RendererMan.Resize(data.m_SizeWidth, data.m_SizeHeight);

                    break;
                }

                case WINDOW_EVENT_MOUSE_POSITION:
                {
                    auto &io = ImGui::GetIO();
                    io.MousePos.x = data.m_MouseX;
                    io.MousePos.y = data.m_MouseY;

                    break;
                }

                case WINDOW_EVENT_MOUSE_STATE:
                {
                    auto &io = ImGui::GetIO();
                    io.MouseDown[0] = !(bool)data.m_MouseState;

                    break;
                }

                default: break;
            }
        }
    }

    void Engine::BeginFrame()
    {
        ZoneScoped;

        m_Window.Poll();
        DispatchEvents();

        m_ImGui.NewFrame(m_Window.m_Width, m_Window.m_Height);
    }

    void Engine::EndFrame()
    {
        ZoneScoped;

        m_ImGui.EndFrame();
        m_RendererMan.Poll();
    }

}  // namespace lr