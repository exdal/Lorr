#include "Win32Window.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Input/Key.hh"

namespace lr {

static LRESULT CALLBACK LRWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void Win32Window::init(const WindowDesc &desc)
{
    ZoneScoped;

    m_using_monitor = desc.m_current_monitor;
    m_instance = GetModuleHandleA(NULL);

    init_displays();

    SystemMetrics::Display *current_display = get_display(m_using_monitor);
    if (!current_display) {
        LOG_CRITICAL("DISPLAY{} is not available?", m_using_monitor + 1);
        return;
    }

    LOG_TRACE("Creating WIN32 window \"{}\"<{}, {}>", desc.m_title, desc.m_width, desc.m_height);

    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = LRWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)m_instance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "Lorr";
    wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
    wc.cbWndExtra = sizeof(u8 *);

    RegisterClassEx(&wc);

    DWORD style = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_POPUP | WS_VISIBLE);
    i32 windowPosX = current_display->m_pos_x;
    i32 windowPosY = current_display->m_pos_y;
    i32 windowWidth = desc.m_width;
    i32 windowHeight = desc.m_height;
    i32 adjustedWidth = windowWidth;
    i32 adjustedHeight = windowHeight;

    if (desc.m_flags & WindowFlag::FullScreen) {
        style = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        windowWidth = current_display->m_res_w;
        windowHeight = current_display->m_res_h;

        m_is_fullscreen = true;
    }
    else {
        if (desc.m_flags & WindowFlag::Borderless)
            style = WS_POPUP;

        if (desc.m_flags & WindowFlag::Resizable)
            style |= WS_MAXIMIZEBOX | WS_THICKFRAME;

        RECT rc = { 0, 0, windowWidth, windowHeight };
        AdjustWindowRectEx(&rc, style, 0, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
        adjustedWidth = rc.right - rc.left;
        adjustedHeight = rc.bottom - rc.top;

        if (desc.m_flags & WindowFlag::Centered) {
            windowPosX += (current_display->m_res_w / 2) - (adjustedWidth / 2);
            windowPosY += (current_display->m_res_h / 2) - (adjustedHeight / 2);
        }
    }

    m_handle = CreateWindowExA(
        0,
        wc.lpszClassName,
        desc.m_title.data(),
        style,
        windowPosX,
        windowPosY,
        adjustedWidth,
        adjustedHeight,
        nullptr,
        nullptr,
        (HINSTANCE)m_instance,
        this);

    SetWindowLongPtrA((HWND)m_handle, 0, (LONG_PTR)this);

    i32 swFlags = SW_SHOW;
    if (desc.m_flags & WindowFlag::Maximized)
        swFlags = SW_SHOWMAXIMIZED;

    ShowWindow((HWND)m_handle, swFlags);
    UpdateWindow((HWND)m_handle);  // call WM_PAINT

    m_width = windowWidth;
    m_height = windowHeight;
}

void Win32Window::init_displays()
{
    ZoneScoped;

    /// Get info of display[n]
    for (u32 currentDevice = 0; currentDevice < GetSystemMetrics(SM_CMONITORS); currentDevice++) {
        DEVMODE devMode = {};

        DISPLAY_DEVICE displayDevice = {};
        displayDevice.cb = sizeof(DISPLAY_DEVICE);

        DISPLAY_DEVICE monitorName = {};
        monitorName.cb = sizeof(DISPLAY_DEVICE);

        EnumDisplayDevices(NULL, currentDevice, &displayDevice, 0);
        EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode);
        EnumDisplayDevices(displayDevice.DeviceName, 0, &monitorName, 0);

        SystemMetrics::Display display = {
            .m_name = monitorName.DeviceString,
            .m_res_w = devMode.dmPelsWidth,
            .m_res_h = devMode.dmPelsHeight,
            .m_pos_x = devMode.dmPosition.x,
            .m_pos_y = devMode.dmPosition.y,
            .m_refresh_rate = devMode.dmDisplayFrequency,
        };

        m_system_metrics.m_displays[currentDevice] = display;
    }
}

void Win32Window::set_cursor(WindowCursor cursor)
{
    ZoneScoped;

    if (m_current_cursor == WindowCursor::Hidden && cursor != WindowCursor::Hidden)
        ::ShowCursor(true);

    m_current_cursor = cursor;

    if (cursor == WindowCursor::Hidden) {
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

    Win32Window *window = (Win32Window *)GetWindowLongPtrA(hWnd, 0);
    if (!window)  // User data is not set yet, return default proc
        return DefWindowProcA(hWnd, msg, wParam, lParam);

    WindowEventData event_data = {};

    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT: {
            window->m_should_close = true;
            window->push_event(LR_WINDOW_EVENT_QUIT, event_data);
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK: {
            Key button = LR_KEY_NONE;
            if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
                button = LR_KEY_LMOUSE;
            if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK)
                button = LR_KEY_RMOUSE;
            if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK)
                button = LR_KEY_MMOUSE;
            if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK)
                button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? LR_KEY_XMOUSE1 : LR_KEY_XMOUSE2;

            event_data.m_mouse = button;
            event_data.m_mouse_state = LR_MOUSE_STATE_DOWN;
            window->push_event(LR_WINDOW_EVENT_MOUSE_STATE, event_data);
            break;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP: {
            Key button = LR_KEY_NONE;
            if (msg == WM_LBUTTONUP)
                button = LR_KEY_LMOUSE;
            if (msg == WM_RBUTTONUP)
                button = LR_KEY_RMOUSE;
            if (msg == WM_MBUTTONUP)
                button = LR_KEY_MMOUSE;
            if (msg == WM_XBUTTONUP)
                button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? LR_KEY_XMOUSE1 : LR_KEY_XMOUSE2;

            event_data.m_mouse = button;
            event_data.m_mouse_state = LR_MOUSE_STATE_UP;
            window->push_event(LR_WINDOW_EVENT_MOUSE_STATE, event_data);
            break;
        }
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE: {
            event_data.m_mouse_x = LOWORD(lParam);
            event_data.m_mouse_y = HIWORD(lParam);
            window->push_event(LR_WINDOW_EVENT_MOUSE_POSITION, event_data);
            break;
        }
        case WM_KEYDOWN:
            event_data.m_key = LR_KEY_NONE;
            event_data.m_key_state = LR_KEY_STATE_DOWN;
            window->push_event(LR_WINDOW_EVENT_KEYBOARD_STATE, event_data);
            break;

        case WM_KEYUP:
            event_data.m_key = LR_KEY_NONE;
            event_data.m_key_state = LR_KEY_STATE_UP;
            window->push_event(LR_WINDOW_EVENT_KEYBOARD_STATE, event_data);
            break;

        case WM_ENTERSIZEMOVE:
            window->m_size_ended = false;
            break;

        case WM_EXITSIZEMOVE: {
            RECT rc;
            GetClientRect((HWND)window->m_handle, &rc);

            window->m_size_ended = true;

            event_data.m_size_width = rc.right;
            event_data.m_size_height = rc.bottom;
            window->push_event(LR_WINDOW_EVENT_RESIZE, event_data);

            break;
        }

        case WM_SIZE: {
            if (window->m_size_ended && wParam != SIZE_MINIMIZED) {
                event_data.m_size_width = (u32)LOWORD(lParam);
                event_data.m_size_height = (u32)HIWORD(lParam);
                window->push_event(LR_WINDOW_EVENT_RESIZE, event_data);
            }

            break;
        }

        case WM_SETCURSOR: {
            if (LOWORD(lParam) == 1) {
                window->set_cursor(window->m_current_cursor);
                break;
            }

            return DefWindowProcA(hWnd, msg, wParam, lParam);
        }

        default:
            return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    return false;
}

void Win32Window::poll()
{
    ZoneScoped;

    MSG msg;

    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

}  // namespace lr
