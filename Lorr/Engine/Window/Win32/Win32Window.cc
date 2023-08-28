// Created on Thursday May 5th 2022 by exdal
// Last modified on Monday August 28th 2023 by exdal

#include "Win32Window.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Engine.hh"
#include "Input/Key.hh"

namespace lr
{

static LRESULT CALLBACK LRWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void Win32Window::Init(const WindowDesc &desc)
{
    ZoneScoped;

    m_UsingMonitor = desc.m_CurrentMonitor;
    m_pInstance = GetModuleHandleA(NULL);

    InitDisplays();

    SystemMetrics::Display *pCurrentDisplay = GetDisplay(m_UsingMonitor);
    if (!pCurrentDisplay)
    {
        LOG_CRITICAL("DISPLAY{} is not available?", m_UsingMonitor + 1);
        return;
    }

    LOG_TRACE("Creating WIN32 window \"{}\"<{}, {}>", desc.m_Title, desc.m_Width, desc.m_Height);

    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = LRWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)m_pInstance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "Lorr";
    wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
    wc.cbWndExtra = sizeof(u8 *);

    RegisterClassEx(&wc);

    DWORD style = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_POPUP | WS_VISIBLE);
    i32 windowPosX = pCurrentDisplay->m_PosX;
    i32 windowPosY = pCurrentDisplay->m_PosY;
    i32 windowWidth = desc.m_Width;
    i32 windowHeight = desc.m_Height;
    i32 adjustedWidth = windowWidth;
    i32 adjustedHeight = windowHeight;

    if (desc.m_Flags & WindowFlag::FullScreen)
    {
        style = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        windowWidth = pCurrentDisplay->m_ResW;
        windowHeight = pCurrentDisplay->m_ResH;

        m_IsFullscreen = true;
    }
    else
    {
        if (desc.m_Flags & WindowFlag::Borderless)
            style = WS_POPUP;

        if (desc.m_Flags & WindowFlag::Resizable)
            style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

        RECT rc = { 0, 0, windowWidth, windowHeight };
        AdjustWindowRectEx(&rc, style, 0, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
        adjustedWidth = rc.right - rc.left;
        adjustedHeight = rc.bottom - rc.top;

        if (desc.m_Flags & WindowFlag::Centered)
        {
            windowPosX += (pCurrentDisplay->m_ResW / 2) - (adjustedWidth / 2);
            windowPosY += (pCurrentDisplay->m_ResH / 2) - (adjustedHeight / 2);
        }
    }

    m_pHandle = CreateWindowExA(
        0,
        wc.lpszClassName,
        desc.m_Title.data(),
        style,
        windowPosX,
        windowPosY,
        adjustedWidth,
        adjustedHeight,
        nullptr,
        nullptr,
        (HINSTANCE)m_pInstance,
        this);

    SetWindowLongPtrA((HWND)m_pHandle, 0, (LONG_PTR)this);

    i32 swFlags = SW_SHOW;
    if (desc.m_Flags & WindowFlag::Maximized)
        swFlags = SW_SHOWMAXIMIZED;

    ShowWindow((HWND)m_pHandle, swFlags);
    UpdateWindow((HWND)m_pHandle);  // call WM_PAINT

    m_Width = windowWidth;
    m_Height = windowHeight;
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

        SystemMetrics::Display display = {
            .m_Name = monitorName.DeviceString,
            .m_ResW = devMode.dmPelsWidth,
            .m_ResH = devMode.dmPelsHeight,
            .m_PosX = devMode.dmPosition.x,
            .m_PosY = devMode.dmPosition.y,
            .m_RefreshRate = devMode.dmDisplayFrequency,
        };

        m_SystemMetrics.m_Displays[currentDevice] = display;
    }
}

void Win32Window::SetCursor(WindowCursor cursor)
{
    ZoneScoped;

    if (m_CurrentCursor == WindowCursor::Hidden && cursor != WindowCursor::Hidden)
        ::ShowCursor(true);

    m_CurrentCursor = cursor;

    if (cursor == WindowCursor::Hidden)
    {
        ::ShowCursor(false);
        return;
    }

    static const HCURSOR kCursors[] = {
        LoadCursor(NULL, IDC_ARROW),     // Arrow
        LoadCursor(NULL, IDC_IBEAM),     // TextInput
        LoadCursor(NULL, IDC_SIZEALL),   // ResizeAll
        LoadCursor(NULL, IDC_SIZENS),    // ResizeNS
        LoadCursor(NULL, IDC_SIZEWE),    // ResizeEW
        LoadCursor(NULL, IDC_SIZENESW),  // ResizeNESW
        LoadCursor(NULL, IDC_SIZENWSE),  // ResizeNWSE
        LoadCursor(NULL, IDC_HAND),      // Hand
        LoadCursor(NULL, IDC_NO),        // NotAllowed
    };

    ::SetCursor(kCursors[(u32)cursor]);
}

LRESULT CALLBACK LRWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ZoneScoped;

    Win32Window *pWindow = (Win32Window *)GetWindowLongPtrA(hWnd, 0);
    if (!pWindow)  // User data is not set yet, return default proc
        return DefWindowProcA(hWnd, msg, wParam, lParam);

    Engine *pEngine = Engine::Get();
    EngineEventData eventData = {};

    switch (msg)
    {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT:
            pWindow->m_ShouldClose = true;
            pEngine->PushEvent(ENGINE_EVENT_QUIT, eventData);
            break;

        case WM_LBUTTONDOWN:
            eventData.m_Mouse = LR_KEY_LMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_DOWN;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_LBUTTONUP:
            eventData.m_Mouse = LR_KEY_LMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_UP;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_LBUTTONDBLCLK:
            eventData.m_Mouse = LR_KEY_LMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_DOUBLE_CLICK;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_RBUTTONDOWN:
            eventData.m_Mouse = LR_KEY_RMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_DOWN;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_RBUTTONUP:
            eventData.m_Mouse = LR_KEY_RMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_UP;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_RBUTTONDBLCLK:
            eventData.m_Mouse = LR_KEY_RMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_DOUBLE_CLICK;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_MBUTTONDOWN:
            eventData.m_Mouse = LR_KEY_MMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_DOWN;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_MBUTTONUP:
            eventData.m_Mouse = LR_KEY_MMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_UP;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_MBUTTONDBLCLK:
            eventData.m_Mouse = LR_KEY_MMOUSE;
            eventData.m_MouseState = LR_MOUSE_STATE_DOUBLE_CLICK;
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_STATE, eventData);
            break;

        case WM_MOUSEMOVE:
            eventData.m_MouseX = LOWORD(lParam);
            eventData.m_MouseY = HIWORD(lParam);
            pEngine->PushEvent(ENGINE_EVENT_MOUSE_POSITION, eventData);
            break;

        case WM_KEYDOWN:
            eventData.m_Key = LR_KEY_NONE;
            eventData.m_KeyState = LR_KEY_STATE_DOWN;
            pEngine->PushEvent(ENGINE_EVENT_KEYBOARD_STATE, eventData);
            break;

        case WM_KEYUP:
            eventData.m_Key = LR_KEY_NONE;
            eventData.m_KeyState = LR_KEY_STATE_UP;
            pEngine->PushEvent(ENGINE_EVENT_KEYBOARD_STATE, eventData);
            break;

        case WM_ENTERSIZEMOVE:
            pWindow->m_SizeEnded = false;
            break;

        case WM_EXITSIZEMOVE:
        {
            RECT rc;
            GetClientRect((HWND)pWindow->m_pHandle, &rc);

            pWindow->m_SizeEnded = true;

            eventData.m_SizeWidth = rc.right;
            eventData.m_SizeHeight = rc.bottom;
            pEngine->PushEvent(ENGINE_EVENT_RESIZE, eventData);

            break;
        }

        case WM_SIZE:
        {
            if (pWindow->m_SizeEnded && wParam != SIZE_MINIMIZED)
            {
                eventData.m_SizeWidth = (u32)LOWORD(lParam);
                eventData.m_SizeHeight = (u32)HIWORD(lParam);
                pEngine->PushEvent(ENGINE_EVENT_RESIZE, eventData);
            }

            break;
        }

        case WM_SETCURSOR:
        {
            if (LOWORD(lParam) == 1)
            {
                pWindow->SetCursor(pWindow->m_CurrentCursor);
                break;
            }

            return DefWindowProcA(hWnd, msg, wParam, lParam);
        }

        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    return false;
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