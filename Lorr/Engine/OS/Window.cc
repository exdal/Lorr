#include "Window.hh"

#include "Engine/Core/Application.hh"

#if defined(LS_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(LS_LINUX)
#if defined(LR_WAYLAND)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif
#endif

#include <GLFW/glfw3native.h>

#if defined(LS_WINDOWS)
#include <vulkan/vulkan_win32.h>
#elif defined(LS_LINUX)
#if defined(LR_WAYLAND)
#include <vulkan/vulkan_wayland.h>
#else
#include <vulkan/vulkan_xlib.h>
#endif
#endif

namespace lr {
bool Window::init(this Window &self, const WindowInfo &info) {
    ZoneScoped;

    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW!");
        return false;
    }

    self.monitor_id = info.monitor;
    i32 new_pos_x = 25;
    i32 new_pos_y = 25;

    if (info.flags & WindowFlag::Centered) {
        SystemDisplay display = self.display_at(self.monitor_id);

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
    self.handle = glfwCreateWindow(static_cast<i32>(info.width), static_cast<i32>(info.height), info.title.data(), nullptr, nullptr);

    /// Initialize callbacks
    glfwSetWindowUserPointer(self.handle, &self);

    glfwSetFramebufferSizeCallback(self.handle, [](GLFWwindow *window, i32 width, i32 height) {
        Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        self->width = width;
        self->height = height;

        Application::get().push_event(
            ApplicationEvent::WindowResize,
            {
                .size = { static_cast<u32>(width), static_cast<u32>(height) },
            });
    });

    glfwSetCursorPosCallback(self.handle, [](GLFWwindow *window, f64 pos_x, f64 pos_y) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::MousePosition,
            {
                .position = { pos_x, pos_y },
            });
    });

    glfwSetMouseButtonCallback(self.handle, [](GLFWwindow *window, i32 button, i32 action, i32 mods) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::MouseState,
            {
                .key = static_cast<Key>(button),
                .key_state = static_cast<KeyState>(action),
                .key_mods = static_cast<KeyMod>(mods),
            });
    });

    glfwSetKeyCallback(self.handle, [](GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::KeyboardState,
            {
                .key = static_cast<Key>(key),
                .key_state = static_cast<KeyState>(action),
                .key_mods = static_cast<KeyMod>(mods),
                .key_scancode = scancode,
            });
    });

    glfwSetScrollCallback(self.handle, [](GLFWwindow *window, f64 off_x, f64 off_y) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::MouseScroll,
            {
                .position = { off_x, off_y },
            });
    });

    // LR_WINDOW_EVENT_CHAR
    glfwSetCharCallback(self.handle, [](GLFWwindow *window, u32 codepoint) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::InputChar,
            {
                .input_char = static_cast<c32>(codepoint),
            });
    });

    // LR_WINDOW_EVENT_DROP
    glfwSetDropCallback(self.handle, [](GLFWwindow *window, i32 path_count, const c8 *paths_cstr[]) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        std::vector<std::string> paths(path_count);
        for (i32 i = 0; i < path_count; i++) {
            paths[i] = std::string(paths_cstr[i]);
        }

        Application::get().push_event(
            ApplicationEvent::Drop,
            {
                .paths = paths,
            });
    });

    glfwSetWindowCloseCallback(self.handle, [](GLFWwindow *window) {
        [[maybe_unused]] Window *self = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(ApplicationEvent::Quit, {});
    });

    /// Initialize standard cursors, should be available across all platforms
    /// hopefully...
    self.cursors = {
        glfwCreateStandardCursor(GLFW_CURSOR_NORMAL),      glfwCreateStandardCursor(GLFW_IBEAM_CURSOR),     glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_NS_CURSOR),   glfwCreateStandardCursor(GLFW_RESIZE_EW_CURSOR), glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR), glfwCreateStandardCursor(GLFW_HAND_CURSOR),      glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR),
        glfwCreateStandardCursor(GLFW_CURSOR_HIDDEN),
    };

    i32 real_width;
    i32 real_height;
    glfwGetWindowSize(self.handle, &real_width, &real_height);

    self.width = real_width;
    self.height = real_height;

    return true;
}

void Window::poll(this Window &) {
    ZoneScoped;

    glfwPollEvents();
}

void Window::set_cursor(this Window &self, WindowCursor cursor) {
    glfwSetCursor(self.handle, self.cursors[usize(cursor)]);
}

SystemDisplay Window::display_at(this Window &, u32 monitor_id) {
    ZoneScoped;

    GLFWmonitor *monitor = nullptr;
    if (monitor_id == WindowInfo::USE_PRIMARY_MONITOR) {
        monitor = glfwGetPrimaryMonitor();
    } else {
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

ls::result<VkSurfaceKHR, VkResult> Window::get_surface(this Window &self, VkInstance instance) {
    VkSurfaceKHR surface = nullptr;

#if defined(LS_WINDOWS)
    auto vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = GetModuleHandleA(nullptr),
        .hwnd = glfwGetWin32Window(self.handle),
    };

    auto result = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface);
#elif defined(LS_LINUX)
#if defined(LR_WAYLAND)
    auto vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");

    VkWaylandSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .display = glfwGetWaylandDisplay(),
        .surface = glfwGetWaylandWindow(self.handle),
    };

    auto result = vkCreateWaylandSurfaceKHR(instance, &create_info, nullptr, &surface);
#else
    auto vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");

    VkXlibSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .dpy = glfwGetX11Display(),
        .window = glfwGetX11Window(self.handle),
    };

    auto result = vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface);
#endif
#endif

    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create window surface! {}", static_cast<i32>(result));
        return result;
    }

    return ls::result(surface, VK_SUCCESS);
}

bool Window::should_close(this Window &self) {
    return glfwWindowShouldClose(self.handle);
}
}  // namespace lr
