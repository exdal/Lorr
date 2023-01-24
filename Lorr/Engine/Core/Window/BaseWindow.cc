#include "BaseWindow.hh"

#include "Engine.hh"

#undef LOG_SET_NAME
#define LOG_SET_NAME "WINDOW"

namespace lr
{
    SystemMetrics::Display *BaseWindow::GetDisplay(u32 monitor)
    {
        if (monitor >= SystemMetrics::kMaxSupportedDisplay)
            return nullptr;

        return &m_SystemMetrics.m_Displays[monitor];
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