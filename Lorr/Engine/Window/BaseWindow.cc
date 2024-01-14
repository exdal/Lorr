#include "BaseWindow.hh"

namespace lr
{
void BaseWindow::poll()
{
    ZoneScoped;
    while (m_event_manager.peek())
    {
        WindowEventData data = {};
        Event event = m_event_manager.dispatch(data);

        switch (event)
        {
            case LR_WINDOW_EVENT_QUIT:
            {
                m_should_close = true;
                break;
            }
            case LR_WINDOW_EVENT_RESIZE:
            {
                m_width = data.m_size_width;
                m_height = data.m_size_height;
                break;
            }
            default:
                break;
        }
    }
}

void BaseWindow::push_event(Event event, WindowEventData &data)
{
    ZoneScoped;

    m_event_manager.push(event, data);
}

SystemMetrics::Display *BaseWindow::get_display(u32 monitor)
{
    if (monitor >= SystemMetrics::k_max_supported_display)
        return nullptr;

    return &m_system_metrics.m_displays[monitor];
}

bool BaseWindow::should_close()
{
    return m_should_close;
}
}  // namespace lr