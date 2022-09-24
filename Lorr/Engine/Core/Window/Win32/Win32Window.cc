#include "Win32Window.hh"

namespace lr
{
    void Win32Window::NativeInit(const WindowDesc &desc)
    {
        ZoneScoped;

        auto &flags = desc.Flags;

        SystemMetrics::Display *pCurrentDisplay = GetDisplay(m_UsingMonitor);
        if (!pCurrentDisplay)
        {
            LOG_CRITICAL("DISPLAY{} is not available?", m_UsingMonitor + 1);
            return;
        }

        if (m_Width == 0 && m_Height == 0)
        {
            m_Width = pCurrentDisplay->ResW;
            m_Height = pCurrentDisplay->ResH;
        }

        LOG_TRACE("Creating WIN32 window \"{}\"<{}, {}>", desc.Title, m_Width, m_Height);

        WNDCLASSEX wc;
        ZeroMemory(&wc, sizeof(WNDCLASSEX));
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = WindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = m_Instance;
        wc.hIcon = LoadIcon(0, IDI_APPLICATION);
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszMenuName = 0;
        wc.lpszClassName = "Lorr";
        wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
        wc.cbWndExtra = sizeof(u8 *);

        RegisterClassEx(&wc);

        DWORD style = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_POPUP | WS_VISIBLE);
        u32 windowPosX = pCurrentDisplay->PosX;
        u32 windowPosY = pCurrentDisplay->PosY;
        u32 windowWidth = 0;
        u32 windowHeight = 0;

        if (flags & WindowFlags::Fullscreen)
        {
            style = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

            DEVMODE dm = {};

            dm.dmSize = sizeof(DEVMODE);
            dm.dmPelsWidth = m_Width;
            dm.dmPelsHeight = m_Height;
            dm.dmBitsPerPel = 32;
            dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

            if (ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
                // throw error
                LOG_ERROR("Fullscreen is not supported by the GPU.");
            }

            windowWidth = pCurrentDisplay->ResW;
            windowHeight = pCurrentDisplay->ResH;

            m_IsFullscreen = true;
        }
        else
        {
            if (flags & WindowFlags::Borderless)
            {
                style = WS_POPUP;

                windowWidth = pCurrentDisplay->ResW;
                windowHeight = pCurrentDisplay->ResH;
            }

            if (flags & WindowFlags::Resizable)
                style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

            RECT rc = { 0, 0, (long)m_Width, (long)m_Height };
            AdjustWindowRectEx(&rc, style, 0, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
            windowWidth = rc.right - rc.left;
            windowHeight = rc.bottom - rc.top;

            if (flags & WindowFlags::Centered)
            {
                windowPosX += (pCurrentDisplay->ResW / 2) - (windowWidth / 2);
                windowPosY += (pCurrentDisplay->ResH / 2) - (windowHeight / 2);
            }
        }

        m_Handle =
            CreateWindowExA(0, wc.lpszClassName, desc.Title.data(), style, windowPosX, windowPosY, windowWidth, windowHeight, 0, 0, m_Instance, this);

        i32 swFlags = SW_SHOW;
        if (flags & WindowFlags::Maximized)
            swFlags = SW_SHOWMAXIMIZED;

        ShowWindow(m_Handle, swFlags);
        UpdateWindow(m_Handle);  // call WM_PAINT

        // Set user data
        SetWindowLongPtrA(m_Handle, 0, (LONG_PTR)this);
    }

    void Win32Window::GetDisplays()
    {
        ZoneScoped;

        /// Get info of display[n]
        for (u32 currentDevice = 0; currentDevice < GetSystemMetrics(SM_CMONITORS); currentDevice++)
        {
            DEVMODE devMode = {};

            DISPLAY_DEVICE displayDevice = {};
            displayDevice.cb = sizeof(DISPLAY_DEVICE);

            DISPLAY_DEVICE monitorName = {};
            monitorName.cb = sizeof(DISPLAY_DEVICE);

            EnumDisplayDevices(NULL, currentDevice, &displayDevice, 0);
            EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode);
            EnumDisplayDevices(displayDevice.DeviceName, 0, &monitorName, 0);

            SystemMetrics::Display display;
            display.Name = monitorName.DeviceString;
            display.RefreshRate = devMode.dmDisplayFrequency;
            display.ResW = devMode.dmPelsWidth;
            display.ResH = devMode.dmPelsHeight;
            display.PosX = devMode.dmPosition.x;
            display.PosY = devMode.dmPosition.y;

            m_SystemMetrics.Displays[currentDevice] = display;
        }
    }

    void Win32Window::SetCursor(Cursor cursor)
    {
        ZoneScoped;
    }

    LRESULT CALLBACK Win32Window::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        ZoneScoped;

        Win32Window *pWindow = (Win32Window *)GetWindowLongPtrA(hWnd, 0);
        if (!pWindow)  // User data is not set yet, return default proc
            return DefWindowProcA(hWnd, msg, wParam, lParam);

        switch (msg)
        {
            // Those terminate messages are different but we will leave it like that for now
            // Go to the end of file to see more info
            case WM_CLOSE:
            case WM_DESTROY:
            case WM_QUIT: pWindow->m_ShouldClose = true; break;

            case WM_ENTERSIZEMOVE:
            {
                pWindow->m_SizeEnded = false;

                break;
            }
            case WM_EXITSIZEMOVE:
            {
                RECT rc;
                GetClientRect(pWindow->m_Handle, &rc);

                pWindow->m_SizeEnded = true;
                pWindow->OnSizeChanged(rc.right, rc.bottom);

                break;
            }
            case WM_SIZE:
            {
                if (pWindow->m_SizeEnded && wParam != SIZE_MINIMIZED)
                    pWindow->OnSizeChanged((u32)LOWORD(lParam), (u32)HIWORD(lParam));

                break;
            }

            default: return DefWindowProcA(hWnd, msg, wParam, lParam);
        }

        return FALSE;
    }

    void Win32Window::Poll()
    {
        ZoneScoped;

        MSG msg;

        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

}  // namespace lr

/*
    WM_CLOSE:
        Sent as a signal that a window or an application should terminate.

    WM_DESTROY:
        Sent when a window is being destroyed. It is sent to the window procedure of the window being destroyed after the window is removed
        from the screen.

        This message is sent first to the window being destroyed and then to the child windows (if any) as they are destroyed. During the
        processing of the message, it can be assumed that all child windows still exist.

    WM_QUIT:
        Indicates a request to terminate an application, and is generated when the application calls the PostQuitMessage function. This
        message causes the GetMessage function to return zero.
*/