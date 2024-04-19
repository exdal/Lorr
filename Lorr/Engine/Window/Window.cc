#include "Window.hh"

namespace lr::window {
SystemDisplay *Window::get_display(u32 monitor)
{
    if (monitor >= m_displays.max_size())
        return nullptr;

    return &m_displays[monitor];
}

bool Window::should_close()
{
    return m_should_close;
}
}  // namespace lr::window
