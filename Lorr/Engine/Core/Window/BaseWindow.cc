#include "BaseWindow.hh"

#include "Engine.hh"

namespace lr
{
    SystemMetrics::Display *BaseWindow::GetDisplay(u32 monitor)
    {
        if (monitor >= SystemMetrics::kMaxSupportedDisplay)
            return nullptr;

        return &m_SystemMetrics.Displays[monitor];
    }

    bool BaseWindow::ShouldClose()
    {
        return m_ShouldClose;
    }

    void BaseWindow::OnSizeChanged(u32 width, u32 height)
    {
        LOG_TRACE("Window size changed to W: {}, H: {}.", width, height);

        m_Width = width;
        m_Height = height;
    }

}  // namespace lr