#include "Window.hh"

#if defined(LR_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(LR_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#if defined(LR_WIN32)
#include <vulkan/vulkan_win32.h>
#elif defined(LR_LINUX)
#include <vulkan/vulkan_xlib.h>
#endif

namespace lr::os {
Window::~Window()
{
    ZoneScoped;

    glfwTerminate();
}

bool Window::init(const WindowInfo &info)
{
    ZoneScoped;

    if (!glfwInit()) {
        LR_LOG_ERROR("Failed to initialize GLFW!");
        return false;
    }

    m_monitor_id = info.monitor;
    i32 new_pos_x = 25;
    i32 new_pos_y = 25;

    if (info.flags & WindowFlag::Centered) {
        SystemDisplay display = get_display(m_monitor_id);

        i32 dc_x = static_cast<i32>(display.res_w / 2);
        i32 dc_y = static_cast<i32>(display.res_h / 2);

        new_pos_x += dc_x - static_cast<i32>(info.width / 2);
        new_pos_y += dc_y - static_cast<i32>(info.height / 2);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, !!(info.flags & WindowFlag::Resizable));
    glfwWindowHint(GLFW_DECORATED, !(info.flags & WindowFlag::Borderless));
    glfwWindowHint(GLFW_MAXIMIZED, !!(info.flags & WindowFlag::Maximized));
    glfwWindowHint(GLFW_POSITION_X, new_pos_x);
    glfwWindowHint(GLFW_POSITION_Y, new_pos_y);
    m_handle = glfwCreateWindow(info.width, info.height, info.title.data(), nullptr, nullptr);

    /// Initialize callbacks
    glfwSetWindowUserPointer(m_handle, this);

    // LR_WINDOW_EVENT_RESIZE
    glfwSetWindowSizeCallback(m_handle, [](GLFWwindow *window, i32 width, i32 height) {
        Window *this_handle = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        WindowEventManager &event_man = this_handle->m_event_manager;

        WindowEventData event_data = {
            .size_width = static_cast<u32>(width),
            .size_height = static_cast<u32>(height),
        };
        event_man.push(LR_WINDOW_EVENT_RESIZE, event_data);
    });

    // LR_WINDOW_EVENT_MOUSE_POSITION
    glfwSetCursorPosCallback(m_handle, [](GLFWwindow *window, f64 pos_x, f64 pos_y) {
        Window *this_handle = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        WindowEventManager &event_man = this_handle->m_event_manager;

        WindowEventData event_data = {
            .mouse_x = static_cast<f32>(pos_x),
            .mouse_y = static_cast<f32>(pos_y),
        };
        event_man.push(LR_WINDOW_EVENT_MOUSE_POSITION, event_data);
    });

    // LR_WINDOW_EVENT_MOUSE_STATE
    glfwSetMouseButtonCallback(m_handle, [](GLFWwindow *window, i32 button, i32 action, i32 mods) {
        Window *this_handle = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        WindowEventManager &event_man = this_handle->m_event_manager;

        WindowEventData event_data = {
            .mouse = static_cast<Key>(button),
            .mouse_state = static_cast<KeyState>(action),
            .mouse_mods = static_cast<KeyMod>(mods),
        };
        event_man.push(LR_WINDOW_EVENT_MOUSE_STATE, event_data);
    });

    // LR_WINDOW_EVENT_KEYBOARD_STATE
    glfwSetKeyCallback(m_handle, [](GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
        Window *this_handle = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        WindowEventManager &event_man = this_handle->m_event_manager;

        WindowEventData event_data = {
            .key = static_cast<Key>(key),
            .key_state = static_cast<KeyState>(action),
            .key_mods = static_cast<KeyMod>(mods),
            .key_scancode = scancode,
        };
        event_man.push(LR_WINDOW_EVENT_KEYBOARD_STATE, event_data);
    });

    glfwSetCharCallback(m_handle, [](GLFWwindow *window, u32 codepoint) {
        Window *this_handle = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        WindowEventManager &event_man = this_handle->m_event_manager;

        WindowEventData event_data = {
            .char_val = static_cast<char32_t>(codepoint),
        };
        event_man.push(LR_WINDOW_EVENT_CHAR, event_data);
    });

    /// Initialize standard cursors, should be available across all platforms
    /// hopefully...
    m_cursors = {
        glfwCreateStandardCursor(GLFW_CURSOR_NORMAL),      glfwCreateStandardCursor(GLFW_IBEAM_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR),  glfwCreateStandardCursor(GLFW_RESIZE_NS_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_EW_CURSOR),   glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR), glfwCreateStandardCursor(GLFW_HAND_CURSOR),
        glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR), glfwCreateStandardCursor(GLFW_CURSOR_HIDDEN),
    };

    m_width = info.width;
    m_height = info.height;

    return true;
}

void Window::poll()
{
    ZoneScoped;

    glfwPollEvents();
}

void Window::set_cursor(WindowCursor cursor)
{
    glfwSetCursor(m_handle, m_cursors[usize(cursor)]);
}

SystemDisplay Window::get_display(u32 monitor_id)
{
    ZoneScoped;

    GLFWmonitor *monitor = nullptr;
    if (monitor_id == WindowInfo::USE_PRIMARY_MONITOR) {
        monitor = glfwGetPrimaryMonitor();
    }
    else {
        i32 monitor_count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&monitor_count);
        if (monitor_id > static_cast<u32>(monitor_count)) {
            return {};
        }

        monitor = monitors[monitor_id];
    }
    const char *monitor_name = glfwGetMonitorName(monitor);
    const GLFWvidmode *vid_mode = glfwGetVideoMode(monitor);
    i32 pos_x = 0;
    i32 pos_y = 0;
    glfwGetMonitorPos(monitor, &pos_x, &pos_y);

    return SystemDisplay{
        .name = monitor_name,
        .res_w = vid_mode->width,
        .res_h = vid_mode->height,
        .pos_x = pos_x,
        .pos_y = pos_y,
        .refresh_rate = vid_mode->refreshRate,
    };
}

Result<VkSurfaceKHR, VkResult> Window::get_surface(VkInstance instance)
{
    VkSurfaceKHR surface = nullptr;

#if defined(LR_WIN32)
    auto vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = GetModuleHandleA(nullptr),
        .hwnd = glfwGetWin32Window(m_handle),
    };

    auto result = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface);
#elif defined(LR_LINUX)

    auto vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");

    VkXlibSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .dpy = glfwGetX11Display(),
        .window = glfwGetX11Window(m_handle),
    };

    auto result = vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface);
#endif

    if (result != VK_SUCCESS) {
        LR_LOG_ERROR("Failed to create window surface! {}", static_cast<i32>(result));
        return result;
    }

    return Result(surface, VK_SUCCESS);
}

bool Window::should_close()
{
    return glfwWindowShouldClose(m_handle);
}
}  // namespace lr::os
