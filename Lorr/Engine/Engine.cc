#include "Engine.hh"

#include "Core/Config.hh"
#include "Core/Job.hh"
#include "Renderer/Renderer.hh"

namespace lr
{
void Engine::create(EngineDesc &engineDesc)
{
    ZoneScoped;

    Logger::Init();
    Config::Init();
    Job::JobManager::Init(CONFIG_GET_VAR(JM_WORKER_COUNT));
    m_window.init(engineDesc.m_window_desc);
    m_imgui.Init(m_window.m_width, m_window.m_height);
    m_renderer.create(&m_window);
}

void Engine::push_event(Event event, EngineEventData &data)
{
    ZoneScoped;

    m_event_man.push(event, data);
}

void Engine::dispatch_events()
{
    ZoneScopedN("Dispatch Engine Events");

    while (m_event_man.peek())
    {
        EngineEventData data = {};
        Event event = m_event_man.dispatch(data);

        switch (event)
        {
            case ENGINE_EVENT_QUIT:
            {
                m_shutting_down = true;
                break;
            }
            case ENGINE_EVENT_RESIZE:
            {
                m_window.m_width = data.m_size_width;
                m_window.m_height = data.m_size_height;
                m_renderer.on_resize(data.m_size_width, data.m_size_height);
                break;
            }
            case ENGINE_EVENT_MOUSE_POSITION:
            {
                ImGuiIO &io = ImGui::GetIO();
                io.MousePos.x = data.m_mouse_x;
                io.MousePos.y = data.m_mouse_y;
                break;
            }
            case ENGINE_EVENT_MOUSE_STATE:
            {
                ImGuiIO &io = ImGui::GetIO();
                io.MouseDown[data.m_mouse - 1] = !(bool)data.m_mouse_state;
                break;
            }
            case ENGINE_EVENT_CURSOR_STATE:
            {
                m_window.set_cursor(data.m_window_cursor);
                break;
            }
            default:
                break;
        }
    }
}

void Engine::prepare()
{
    ZoneScoped;
}

void Engine::begin_frame()
{
    ZoneScopedN("Engine Begin Frame");

    m_renderer.draw();
}

void Engine::end_frame()
{
    ZoneScopedN("Engine End Frame");

    m_window.poll();
    dispatch_events();
}
}  // namespace lr