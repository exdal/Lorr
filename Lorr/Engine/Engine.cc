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
        m_RendererMan.Init(Renderer::APIType::D3D12, Graphics::APIFlags::None, &m_Window);

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

            m_ImGui.NewFrame(m_Window.m_Width, m_Window.m_Height);

            ImGui::Begin("Dear ImGui Demo", nullptr);

            ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

            // Menu Bar

            ImGui::Text("dear imgui says hello! (%s) (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
            ImGui::Spacing();

            ImGui::Text("ABOUT THIS DEMO:");
            ImGui::BulletText("Sections below are demonstrating many aspects of the library.");
            ImGui::BulletText("The \"Examples\" menu above leads to more demo contents.");
            ImGui::BulletText("The \"Tools\" menu above gives access to: About Box, Style Editor,\n"
                              "and Metrics/Debugger (general purpose Dear ImGui debugging tool).");
            ImGui::Separator();

            ImGui::Text("PROGRAMMER GUIDE:");
            ImGui::BulletText("See the ShowDemoWindow() code in imgui_demo.cpp. <- you are here!");
            ImGui::BulletText("See comments in imgui.cpp.");
            ImGui::BulletText("See example applications in the examples/ folder.");
            ImGui::BulletText("Read the FAQ at http://www.dearimgui.org/faq/");
            ImGui::BulletText("Set 'io.ConfigFlags |= NavEnableKeyboard' for keyboard controls.");
            ImGui::BulletText("Set 'io.ConfigFlags |= NavEnableGamepad' for gamepad controls.");
            ImGui::Separator();

            ImGui::End();

            m_ImGui.EndFrame();

            m_RendererMan.Poll();
        }
    }

}  // namespace lr