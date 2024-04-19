#include "Win32Window.hh"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "Input/Key.hh"

namespace lr::window {
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool Win32Window::init(const WindowInfo &info)
{
    ZoneScoped;

    m_using_display = info.current_monitor;
    m_instance = GetModuleHandleA(nullptr);

    get_displays();
    SystemDisplay *current_display = get_display(m_using_display);

    WNDCLASSEXA window_class = {
        .cbSize = sizeof(WNDCLASSEXA),
        .style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        .lpfnWndProc = WindowProc,
        .cbClsExtra = 0,
        .cbWndExtra = sizeof(Win32Window *),
        .hInstance = (HINSTANCE)m_instance,
        .hIcon = LoadIconA(nullptr, IDI_APPLICATION),
        .hCursor = LoadCursorA(nullptr, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),
        .lpszMenuName = nullptr,
        .lpszClassName = "Lorr Window",
        .hIconSm = LoadIconA(nullptr, IDI_APPLICATION),
    };
    RegisterClassExA(&window_class);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_POPUP | WS_VISIBLE;
    i32 pos_x = 0;
    i32 pos_y = 0;
    u32 width = info.width;
    u32 height = info.height;

    if (info.flags & WindowFlag::Resizable)
        style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
    if (info.flags & WindowFlag::Borderless)
        style = WS_POPUP;

    RECT window_rect = { pos_x, pos_y, (i32)width, (i32)height };
    AdjustWindowRectEx(&window_rect, style, false, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
    width = window_rect.right - window_rect.left;
    height = window_rect.bottom - window_rect.top;

    if (info.flags & WindowFlag::Centered) {
        i32 dc_x = static_cast<i32>(current_display->res_w / 2);
        i32 dc_y = static_cast<i32>(current_display->res_h / 2);

        pos_x += dc_x - static_cast<i32>(width / 2);
        pos_y += dc_y - static_cast<i32>(height / 2);
    }

    m_handle = CreateWindowExA(
        0,
        window_class.lpszClassName,
        info.title.data(),
        style,
        pos_x,
        pos_y,
        (i32)width,
        (i32)height,
        nullptr,
        nullptr,
        window_class.hInstance,
        this);

    i32 sw_flags = SW_SHOW;
    if (info.flags & WindowFlag::Maximized)
        sw_flags = SW_SHOWMAXIMIZED;

    ShowWindow((HWND)m_handle, sw_flags);
    UpdateWindow((HWND)m_handle);  // call WM_PAINT

    m_width = width;
    m_height = height;

    return true;
}

void Win32Window::get_displays()
{
    ZoneScoped;

    /// Get info of display[n]
    for (u32 monitor_id = 0; monitor_id < GetSystemMetrics(SM_CMONITORS); monitor_id++) {
        DEVMODE devMode = {};

        DISPLAY_DEVICE displayDevice = {};
        displayDevice.cb = sizeof(DISPLAY_DEVICE);

        DISPLAY_DEVICE monitorName = {};
        monitorName.cb = sizeof(DISPLAY_DEVICE);

        EnumDisplayDevices(nullptr, monitor_id, &displayDevice, 0);
        EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode);
        EnumDisplayDevices(displayDevice.DeviceName, 0, &monitorName, 0);

        SystemDisplay display = {
            .name = monitorName.DeviceString,
            .res_w = devMode.dmPelsWidth,
            .res_h = devMode.dmPelsHeight,
            .pos_x = devMode.dmPosition.x,
            .pos_y = devMode.dmPosition.y,
            .refresh_rate = devMode.dmDisplayFrequency,
        };

        m_displays.push_back(display);
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

VkSurfaceKHR Win32Window::get_surface(VkInstance instance)
{
    VkSurfaceKHR surface = nullptr;
    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = (HINSTANCE)m_instance,
        .hwnd = (HWND)m_handle,
    };

    auto result = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Win32 surface! {}", (u32)result);
        return nullptr;
    }

    return surface;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_CREATE) {
        auto create_struct = reinterpret_cast<CREATESTRUCT *>(lParam);
        auto window = static_cast<Win32Window *>(create_struct->lpCreateParams);
        SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }

    auto window = reinterpret_cast<Win32Window *>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
    if (window == nullptr) {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    thread_local bool size_ended = false;
    WindowEventManager &event_man = window->m_event_manager;
    WindowEventData event_data = {};

    LRESULT result = FALSE;
    switch (msg) {
        case WM_CREATE: {
            event_man.push(LR_WINDOW_EVENT_INIT, event_data);
            break;
        }
        case WM_CLOSE:
        case WM_DESTROY:
        case WM_QUIT: {
            window->m_should_close = true;
            event_man.push(LR_WINDOW_EVENT_QUIT, event_data);
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

            event_data.mouse = button;
            event_data.mouse_state = LR_MOUSE_STATE_DOWN;
            event_man.push(LR_WINDOW_EVENT_MOUSE_STATE, event_data);

            result = TRUE;
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

            event_data.mouse = button;
            event_data.mouse_state = LR_MOUSE_STATE_UP;
            event_man.push(LR_WINDOW_EVENT_MOUSE_STATE, event_data);

            result = TRUE;
            break;
        }
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE: {
            event_data.mouse_x = LOWORD(lParam);
            event_data.mouse_y = HIWORD(lParam);
            event_man.push(LR_WINDOW_EVENT_MOUSE_POSITION, event_data);
            break;
        }
        case WM_KEYDOWN: {
            event_data.key = LR_KEY_NONE;
            event_data.key_state = LR_KEY_STATE_DOWN;
            event_man.push(LR_WINDOW_EVENT_KEYBOARD_STATE, event_data);
            break;
        }
        case WM_KEYUP: {
            event_data.key = LR_KEY_NONE;
            event_data.key_state = LR_KEY_STATE_UP;
            event_man.push(LR_WINDOW_EVENT_KEYBOARD_STATE, event_data);
            break;
        }
        case WM_ENTERSIZEMOVE: {
            size_ended = false;
            break;
        }
        case WM_EXITSIZEMOVE: {
            RECT rc = {};
            GetClientRect((HWND)window->m_handle, &rc);

            size_ended = true;

            event_data.size_width = rc.right;
            event_data.size_height = rc.bottom;
            event_man.push(LR_WINDOW_EVENT_RESIZE, event_data);

            break;
        }
        case WM_SIZE: {
            if (size_ended && wParam != SIZE_MINIMIZED) {
                event_data.size_width = (u32)LOWORD(lParam);
                event_data.size_height = (u32)HIWORD(lParam);
                event_man.push(LR_WINDOW_EVENT_RESIZE, event_data);
            }

            break;
        }
        default:
            result = DefWindowProcA(hWnd, msg, wParam, lParam);
            break;
    }

    return result;
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

}  // namespace lr::window
