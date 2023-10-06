// Created on Wednesday May 4th 2022 by exdal
// Last modified on Thursday September 14th 2023 by exdal

#include "Engine.hh"

#include "Core/Config.hh"
#include "Core/Job.hh"
#include "Graphics/APIContext.hh"
#include "Graphics/Renderer/RenderGraph.hh"

namespace lr
{
void Engine::Init(EngineDesc &engineDesc)
{
    ZoneScoped;

    Logger::Init();
    Config::Init();
    Job::JobManager::Init(CONFIG_GET_VAR(JM_WORKER_COUNT));
    m_Window.Init(engineDesc.m_WindowDesc);
    m_ImGui.Init(m_Window.m_Width, m_Window.m_Height);

    Graphics::APIContextDesc apiDesc = {
#ifdef TRACY_ENABLE
        .m_ImageCount = 1,
#else
        .m_ImageCount = CONFIG_GET_VAR(API_SWAPCHAIN_FRAMES),
#endif
        .m_pTargetWindow = &m_Window
    };

    m_APIContext.Init(apiDesc, nullptr);
    Graphics::RenderGraph graph = {};
    graph.Init(&m_APIContext);

    struct PassData
    {
        u32 _empty;
    };
    graph.CreateGraphicsPass<PassData>("sample");
    graph.CreateGraphicsPass<PassData>("sample2");
    graph.CreateGraphicsPass<PassData>("sample3");
}

void Engine::PushEvent(Event event, EngineEventData &data)
{
    ZoneScoped;

    m_EventMan.Push(event, data);
}

void Engine::DispatchEvents()
{
    ZoneScopedN("Dispatch Engine Events");

    while (m_EventMan.Peek())
    {
        EngineEventData data = {};
        Event event = m_EventMan.Dispatch(data);

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
                // m_API.ResizeSwapChain(data.m_SizeWidth, data.m_SizeHeight);
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
            default:
                break;
        }
    }
}

void Engine::Prepare()
{
    ZoneScoped;
}

void Engine::BeginFrame()
{
    ZoneScopedN("Engine Begin Frame");

    m_Window.Poll();
    DispatchEvents();
}

void Engine::EndFrame()
{
    ZoneScopedN("Engine End Frame");
}

}  // namespace lr