#include "Engine/Window/Window.hh"

#include "Engine/Core/Application.hh"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#if defined(LS_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(LS_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan_core.h>
#if defined(LS_WINDOWS)
#include <vulkan/vulkan_win32.h>
#elif defined(LS_LINUX)
#include <vulkan/vulkan_xlib.h>
#endif

namespace lr {
template<>
struct Handle<Window>::Impl {
    u32 width = 0;
    u32 height = 0;

    WindowCursor current_cursor = WindowCursor::Arrow;
    glm::uvec2 cursor_position = {};

    GLFWwindow *handle = nullptr;
    u32 monitor_id = WindowInfo::USE_PRIMARY_MONITOR;
    std::array<GLFWcursor *, usize(WindowCursor::Count)> cursors = {};
};

auto Window::create(const WindowInfo &info) -> Window {
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW!");
        return Handle(nullptr);
    }

    auto impl = new Impl;
    impl->monitor_id = info.monitor;

    i32 new_pos_x = 25;
    i32 new_pos_y = 25;

    if (info.flags & WindowFlag::Centered) {
        SystemDisplay display = Window::display_at(impl->monitor_id);

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
    impl->handle = glfwCreateWindow(static_cast<i32>(info.width), static_cast<i32>(info.height), info.title.data(), nullptr, nullptr);

    /// Initialize callbacks
    glfwSetWindowUserPointer(impl->handle, impl);
    glfwSetFramebufferSizeCallback(impl->handle, [](GLFWwindow *window, i32 width, i32 height) {
        auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));
        self->width = width;
        self->height = height;

        Application::get().push_event(
            ApplicationEvent::WindowResize,
            {
                .size = { static_cast<u32>(width), static_cast<u32>(height) },
            });
    });

    glfwSetCursorPosCallback(impl->handle, [](GLFWwindow *window, f64 pos_x, f64 pos_y) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::MousePosition,
            {
                .position = { pos_x, pos_y },
            });
    });

    glfwSetMouseButtonCallback(impl->handle, [](GLFWwindow *window, i32 button, i32 action, i32 mods) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::MouseState,
            {
                .key = static_cast<Key>(button),
                .key_state = static_cast<KeyState>(action),
                .key_mods = static_cast<KeyMod>(mods),
            });
    });

    glfwSetKeyCallback(impl->handle, [](GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::KeyboardState,
            {
                .key = static_cast<Key>(key),
                .key_state = static_cast<KeyState>(action),
                .key_mods = static_cast<KeyMod>(mods),
                .key_scancode = scancode,
            });
    });

    glfwSetScrollCallback(impl->handle, [](GLFWwindow *window, f64 off_x, f64 off_y) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::MouseScroll,
            {
                .position = { off_x, off_y },
            });
    });

    glfwSetCharCallback(impl->handle, [](GLFWwindow *window, u32 codepoint) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(
            ApplicationEvent::InputChar,
            {
                .input_char = static_cast<c32>(codepoint),
            });
    });

    glfwSetDropCallback(impl->handle, [](GLFWwindow *window, i32 path_count, const c8 *paths_cstr[]) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

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

    glfwSetWindowCloseCallback(impl->handle, [](GLFWwindow *window) {
        [[maybe_unused]] auto *self = reinterpret_cast<Window::Impl *>(glfwGetWindowUserPointer(window));

        Application::get().push_event(ApplicationEvent::Quit, {});
    });

    /// Initialize standard cursors, should be available across all platforms
    /// hopefully...
    impl->cursors = {
        glfwCreateStandardCursor(GLFW_CURSOR_NORMAL),      glfwCreateStandardCursor(GLFW_IBEAM_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR),  glfwCreateStandardCursor(GLFW_RESIZE_NS_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_EW_CURSOR),   glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR),
        glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR), glfwCreateStandardCursor(GLFW_HAND_CURSOR),
        glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR), glfwCreateStandardCursor(GLFW_CURSOR_HIDDEN),
    };

    i32 real_width;
    i32 real_height;
    glfwGetWindowSize(impl->handle, &real_width, &real_height);

    impl->width = real_width;
    impl->height = real_height;

    return { impl };
}

auto Window::poll() -> void {
    ZoneScoped;

    glfwPollEvents();
}

auto Window::set_cursor(WindowCursor cursor) -> void {
    glfwSetCursor(impl->handle, impl->cursors[usize(cursor)]);
}

auto Window::display_at(u32 monitor_id) -> SystemDisplay {
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

auto Window::should_close() -> bool {
    return glfwWindowShouldClose(impl->handle);
}

auto Window::native_handle() -> NativeWindowHandle {
    ZoneScoped;

    return NativeWindowHandle{
#if defined(LS_WINDOWS)
        .win32_instance = GetModuleHandleA(nullptr),
        .win32_handle = glfwGetWin32Window(impl->handle),
#elif defined(LS_LINUX)
        .x11_display = glfwGetX11Display(impl->handle),
        .x11_window = glfwGetX11Window(impl->handle),
#endif
    };
}

}  // namespace lr
