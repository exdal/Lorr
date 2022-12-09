#include "Engine.hh"

#include "Core/Utils/Timer.hh"

namespace lr
{
    void Engine::Init(WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);
        m_ImGui.Init(m_Window.m_Width, m_Window.m_Height);
        m_RendererMan.Init(Renderer::APIType::D3D12, Graphics::APIFlags::None, &m_Window);
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
                    m_Window.m_Width = data.SizeWidth;
                    m_Window.m_Height = data.SizeHeight;

                    m_RendererMan.Resize(data.SizeWidth, data.SizeHeight);

                    break;
                }

                case WINDOW_EVENT_MOUSE_POSITION:
                {
                    auto &io = ImGui::GetIO();
                    io.MousePos.x = data.MouseX;
                    io.MousePos.y = data.MouseY;

                    break;
                }

                case WINDOW_EVENT_MOUSE_STATE:
                {
                    auto &io = ImGui::GetIO();
                    io.MouseDown[0] = !(bool)data.MouseState;

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