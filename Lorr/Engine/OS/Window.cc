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
        LOG_ERROR("Failed to initialize GLFW!");
        return false;
    }

    m_monitor_id = info.monitor;
    i32 pos_x = 25;
    i32 pos_y = 25;

    if (info.flags & WindowFlag::Centered) {
        SystemDisplay display = get_display(m_monitor_id);

        i32 dc_x = static_cast<i32>(display.res_w / 2);
        i32 dc_y = static_cast<i32>(display.res_h / 2);

        pos_x += dc_x - static_cast<i32>(info.width / 2);
        pos_y += dc_y - static_cast<i32>(info.height / 2);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, !!(info.flags & WindowFlag::Resizable));
    glfwWindowHint(GLFW_DECORATED, !(info.flags & WindowFlag::Borderless));
    glfwWindowHint(GLFW_MAXIMIZED, !!(info.flags & WindowFlag::Maximized));
    glfwWindowHint(GLFW_POSITION_X, pos_x);
    glfwWindowHint(GLFW_POSITION_Y, pos_y);
    m_handle = glfwCreateWindow(info.width, info.height, info.title.data(), nullptr, nullptr);

    return true;
}

void Window::poll()
{
    ZoneScoped;

    glfwPollEvents();
}

void Window::set_cursor(WindowCursor cursor)
{
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
    auto vkCreateWin32SurfaceKHR =
        (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = GetModuleHandleA(nullptr),
        .hwnd = glfwGetWin32Window(m_handle),
    };

    auto result = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface);
#elif defined(LR_LINUX)

    auto vkCreateXlibSurfaceKHR =
        (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");

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
        LOG_ERROR("Failed to create window surface! {}", static_cast<i32>(result));
        return result;
    }

    return Result(surface, VK_SUCCESS);
}

bool Window::should_close()
{
    return glfwWindowShouldClose(m_handle);
}
}  // namespace lr::os
