#include "BaseWindow.hh"

#include "Engine.hh"

namespace lr
{
    void BaseWindow::Init(const WindowDesc &desc)
    {
        m_Width = desc.Width;
        m_Height = desc.Height;

        m_UsingMonitor = desc.CurrentMonitor;

        GetDisplays();
        NativeInit(desc);
    }

    SystemMetrics::Display *BaseWindow::GetDisplay(u32 monitor)
    {
        if (monitor >= SystemMetrics::kMaxSupportedDisplay) return nullptr;

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

        GetEngine()->OnWindowResize(width, height);
    }

}  // namespace lr