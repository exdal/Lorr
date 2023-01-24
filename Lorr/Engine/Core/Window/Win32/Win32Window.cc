#include "Win32Window.hh"

#undef LOG_SET_NAME
#define LOG_SET_NAME "WIN32WINDOW"

namespace lr
{
    void Win32Window::Init(const WindowDesc &desc)
    {
        ZoneScoped;

        m_Width = desc.m_Width;
        m_Height = desc.m_Height;

        m_UsingMonitor = desc.m_CurrentMonitor;

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
            m_Width = pCurrentDisplay->m_ResW;
            m_Height = pCurrentDisplay->m_ResH;
        }

        LOG_TRACE("Creating WIN32 window \"{}\"<{}, {}>", desc.m_Title, m_Width, m_Height);

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
        u32 windowPosX = pCurrentDisplay->m_PosX;
        u32 windowPosY = pCurrentDisplay->m_PosY;
        u32 windowWidth = 0;
        u32 windowHeight = 0;

        if (desc.m_Flags & WindowFlags::Fullscreen)
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

            windowWidth = pCurrentDisplay->m_ResW;
            windowHeight = pCurrentDisplay->m_ResH;

            m_IsFullscreen = true;
        }
        else
        {
            if (desc.m_Flags & WindowFlags::Borderless)
            {
                style = WS_POPUP;

                windowWidth = pCurrentDisplay->m_ResW;
                windowHeight = pCurrentDisplay->m_ResH;
            }

            if (desc.m_Flags & WindowFlags::Resizable)
                style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

            RECT rc = { 0, 0, (long)m_Width, (long)m_Height };
            AdjustWindowRectEx(&rc, style, 0, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
            windowWidth = rc.right - rc.left;
            windowHeight = rc.bottom - rc.top;

            if (desc.m_Flags & WindowFlags::Centered)
            {
                windowPosX += (pCurrentDisplay->m_ResW / 2) - (windowWidth / 2);
                windowPosY += (pCurrentDisplay->m_ResH / 2) - (windowHeight / 2);
            }
        }

        m_pHandle =
            CreateWindowExA(0, wc.lpszClassName, desc.m_Title.data(), style, windowPosX, windowPosY, windowWidth, windowHeight, 0, 0, m_Instance, this);

        i32 swFlags = SW_SHOW;
        if (desc.m_Flags & WindowFlags::Maximized)
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
            display.m_Name = monitorName.DeviceString;
            display.m_RefreshRate = devMode.dmDisplayFrequency;
            display.m_ResW = devMode.dmPelsWidth;
            display.m_ResH = devMode.dmPelsHeight;
            display.m_PosX = devMode.dmPosition.x;
            display.m_PosY = devMode.dmPosition.y;

            m_SystemMetrics.m_Displays[currentDevice] = display;
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
                eventData.m_Mouse = LR_KEY_LMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_DOWN;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_LBUTTONUP:
                eventData.m_Mouse = LR_KEY_LMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_UP;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_LBUTTONDBLCLK:
                eventData.m_Mouse = LR_KEY_LMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_DOUBLE_CLICK;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_RBUTTONDOWN:
                eventData.m_Mouse = LR_KEY_RMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_DOWN;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_RBUTTONUP:
                eventData.m_Mouse = LR_KEY_RMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_UP;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_RBUTTONDBLCLK:
                eventData.m_Mouse = LR_KEY_RMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_DOUBLE_CLICK;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MBUTTONDOWN:
                eventData.m_Mouse = LR_KEY_MMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_DOWN;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MBUTTONUP:
                eventData.m_Mouse = LR_KEY_MMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_UP;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MBUTTONDBLCLK:
                eventData.m_Mouse = LR_KEY_MMOUSE;
                eventData.m_MouseState = LR_MOUSE_STATE_DOUBLE_CLICK;
                eventMan.Push(WINDOW_EVENT_MOUSE_STATE, eventData);
                break;

            case WM_MOUSEMOVE:
                eventData.m_MouseX = LOWORD(lParam);
                eventData.m_MouseY = HIWORD(lParam);
                eventMan.Push(WINDOW_EVENT_MOUSE_POSITION, eventData);
                break;

            case WM_KEYDOWN:
                eventData.m_Key = LR_KEY_NONE;
                eventData.m_KeyState = LR_KEY_STATE_DOWN;
                eventMan.Push(WINDOW_EVENT_KEYBOARD_STATE, eventData);
                break;

            case WM_KEYUP:
                eventData.m_Key = LR_KEY_NONE;
                eventData.m_KeyState = LR_KEY_STATE_UP;
                eventMan.Push(WINDOW_EVENT_KEYBOARD_STATE, eventData);
                break;

            case WM_ENTERSIZEMOVE: pWindow->m_SizeEnded = false; break;

            case WM_EXITSIZEMOVE:
            {
                RECT rc;
                GetClientRect((HWND)pWindow->m_pHandle, &rc);

                pWindow->m_SizeEnded = true;

                eventData.m_SizeWidth = rc.right;
                eventData.m_SizeHeight = rc.bottom;
                eventMan.Push(WINDOW_EVENT_RESIZE, eventData);

                break;
            }

            case WM_SIZE:
            {
                if (pWindow->m_SizeEnded && wParam != SIZE_MINIMIZED)
                {
                    eventData.m_SizeWidth = (u32)LOWORD(lParam);
                    eventData.m_SizeHeight = (u32)HIWORD(lParam);
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