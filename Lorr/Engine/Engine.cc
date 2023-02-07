#include "Engine.hh"

#include "Core/Utils/Timer.hh"

namespace lr
{
    void Engine::Init(EngineDesc &engineDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_EventMan.Init();
        m_Window.Init(engineDesc.m_WindowDesc);
        m_ImGui.Init(m_Window.m_Width, m_Window.m_Height);
        m_RendererMan.Init(engineDesc.m_TargetAPI, engineDesc.m_TargetAPIFlags, &m_Window);
    }

    void Engine::PushEvent(Event event, EngineEventData &data)
    {
        ZoneScoped;

        m_EventMan.Push(event, data);
    }

    void Engine::DispatchEvents()
    {
        ZoneScoped;

        u32 eventID = LR_INVALID_EVENT_ID;
        while (m_EventMan.Peek(eventID))
        {
            EngineEventData data = {};
            Event event = m_EventMan.Dispatch(eventID, data);

            switch (event)
            {
                case ENGINE_EVENT_QUIT:
                {
                    m_ShuttingDown = true;

                    break;
                }

                case ENGINE_EVENT_RESIZE:
                {
                    m_Window.m_Width = data.m_SizeWidth;
                    m_Window.m_Height = data.m_SizeHeight;

                    m_RendererMan.Resize(data.m_SizeWidth, data.m_SizeHeight);

                    break;
                }

                case ENGINE_EVENT_MOUSE_POSITION:
                {
                    ImGuiIO &io = ImGui::GetIO();
                    io.MousePos.x = data.m_MouseX;
                    io.MousePos.y = data.m_MouseY;

                    break;
                }

                case ENGINE_EVENT_MOUSE_STATE:
                {
                    ImGuiIO &io = ImGui::GetIO();
                    io.MouseDown[data.m_Mouse - 1] = !(bool)data.m_MouseState;

                    break;
                }

                case ENGINE_EVENT_CURSOR_STATE:
                {
                    m_Window.SetCursor(data.m_WindowCursor);
                    
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

        m_RendererMan.Begin();
    }

    void Engine::EndFrame()
    {
        ZoneScoped;

        m_ImGui.EndFrame();
        m_RendererMan.End();
    }

}  // namespace lr