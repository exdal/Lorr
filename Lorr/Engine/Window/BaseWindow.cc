#include "BaseWindow.hh"

#include "Engine.hh"

namespace lr
{
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

    void BaseWindow::on_size_changed(u32 width, u32 height)
    {
        LOG_TRACE("Window size changed to W: {}, H: {}.", width, height);

        m_width = width;
        m_height = height;
    }

}  // namespace lr