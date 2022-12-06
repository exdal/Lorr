#include "Win32Window.hh"

namespace lr
{
    void Win32Window::Init(const WindowDesc &desc)
    {
        ZoneScoped;

        m_Width = desc.Width;
        m_Height = desc.Height;

        m_UsingMonitor = desc.CurrentMonitor;

        InitDisplays();

        m_EventManager.Init();

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

        if (desc.Flags & WindowFlags::Fullscreen)
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
            if (desc.Flags & WindowFlags::Borderless)
            {
                style = WS_POPUP;

                windowWidth = pCurrentDisplay->ResW;
                windowHeight = pCurrentDisplay->ResH;
            }

            if (desc.Flags & WindowFlags::Resizable)
                style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

            RECT rc = { 0, 0, (long)m_Width, (long)m_Height };
            AdjustWindowRectEx(&rc, style, 0, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
            windowWidth = rc.right - rc.left;
            windowHeight = rc.bottom - rc.top;

            if (desc.Flags & WindowFlags::Centered)
            {
                windowPosX += (pCurrentDisplay->ResW / 2) - (windowWidth / 2);
                windowPosY += (pCurrentDisplay->ResH / 2) - (windowHeight / 2);
            }
        }

        m_pHandle =
            CreateWindowExA(0, wc.lpszClassName, desc.Title.data(), style, windowPosX, windowPosY, windowWidth, windowHeight, 0, 0, m_Instance, this);

        i32 swFlags = SW_SHOW;
        if (desc.Flags & WindowFlags::Maximized)
            swFlags = SW_SHOWMAXIMIZED;

        ShowWindow((HWND)m_pHandle, swFlags);
        UpdateWindow((HWND)m_pHandle);  // call WM_PAINT

        // Set user data
        SetWindowLongPtrA((HWND)m_pHandle, 0, (LONG_PTR)this);
    }

    void Win32Window::InitDisplays()
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

        auto &eventMan = pWindow->m_EventManager;

        WindowEventData eventData = {};

        switch (msg)
        {
            // Those terminate messages are different but we will leave it like that for now
            // Go to the end of file to see more info
            case WM_CLOSE:
            case WM_DESTROY:
            case WM_QUIT:
            {
                pWindow->m_ShouldClose = true;
                eventMan.Push(WINDOW_EVENT_QUIT, eventData);

                break;
            }

            case WM_LBUTTONDOWN:
                eventData.Mouse = Input::LR_KEY_LMOUSE;
                eventData.MouseState = Input::MouseState::Down;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_LBUTTONUP:
                eventData.Mouse = Input::LR_KEY_LMOUSE;
                eventData.MouseState = Input::MouseState::Up;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_LBUTTONDBLCLK:
                eventData.Mouse = Input::LR_KEY_LMOUSE;
                eventData.MouseState = Input::MouseState::DoubleClick;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_RBUTTONDOWN:
                eventData.Mouse = Input::LR_KEY_RMOUSE;
                eventData.MouseState = Input::MouseState::Down;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_RBUTTONUP:
                eventData.Mouse = Input::LR_KEY_RMOUSE;
                eventData.MouseState = Input::MouseState::Up;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_RBUTTONDBLCLK:
                eventData.Mouse = Input::LR_KEY_RMOUSE;
                eventData.MouseState = Input::MouseState::DoubleClick;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MBUTTONDOWN:
                eventData.Mouse = Input::LR_KEY_MMOUSE;
                eventData.MouseState = Input::MouseState::Down;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MBUTTONUP:
                eventData.Mouse = Input::LR_KEY_MMOUSE;
                eventData.MouseState = Input::MouseState::Up;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MBUTTONDBLCLK:
                eventData.Mouse = Input::LR_KEY_MMOUSE;
                eventData.MouseState = Input::MouseState::DoubleClick;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MOUSEMOVE:
                eventData.MouseX = LOWORD(lParam);
                eventData.MouseY = HIWORD(lParam);
                eventMan.Push(WINDOW_EVENT_MOUSE_POSITION, eventData);
                break;

            case WM_KEYDOWN:
                eventData.Key = Input::LR_KEY_NONE;
                eventData.KeyState = Input::KeyState::Down;
                eventMan.Push(WINDOW_EVENT_KEYBOARD_STATE, eventData);
                break;

            case WM_KEYUP:
                eventData.Key = Input::LR_KEY_NONE;
                eventData.KeyState = Input::KeyState::Up;
                eventMan.Push(WINDOW_EVENT_KEYBOARD_STATE, eventData);
                break;

            case WM_ENTERSIZEMOVE: pWindow->m_SizeEnded = false; break;

            case WM_EXITSIZEMOVE:
            {
                RECT rc;
                GetClientRect((HWND)pWindow->m_pHandle, &rc);

                pWindow->m_SizeEnded = true;

                eventData.SizeWidth = rc.right;
                eventData.SizeHeight = rc.bottom;
                eventMan.Push(WINDOW_EVENT_RESIZE, eventData);

                break;
            }

            case WM_SIZE:
            {
                if (pWindow->m_SizeEnded && wParam != SIZE_MINIMIZED)
                {
                    eventData.SizeWidth = (u32)LOWORD(lParam);
                    eventData.SizeHeight = (u32)HIWORD(lParam);
                    eventMan.Push(WINDOW_EVENT_RESIZE, eventData);
                }

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