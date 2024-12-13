#include "Engine/Window/Window.hh"

#include "Engine/Core/Application.hh"
#include "Engine/Graphics/Vulkan/Impl.hh"

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

    SystemDisplay display = Window::display_at(info.monitor);

    i32 new_pos_x = 25;
    i32 new_pos_y = 25;
    i32 new_width = static_cast<i32>(info.width);
    i32 new_height = static_cast<i32>(info.height);

    if (info.flags & WindowFlag::WorkAreaRelative) {
        new_pos_x = display.work_area.x;
        new_pos_y = display.work_area.y;
        new_width = display.work_area.z;
        new_height = display.work_area.w;
    } else if (info.flags & WindowFlag::Centered) {
        auto center_x = display.work_area.z / 2;
        auto center_y = display.work_area.w / 2;

        new_pos_x = center_x - new_width / 2;
        new_pos_y = center_y - new_height / 2;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, !!(info.flags & WindowFlag::Resizable));
    glfwWindowHint(GLFW_DECORATED, !(info.flags & WindowFlag::Borderless));
    glfwWindowHint(GLFW_MAXIMIZED, !!(info.flags & WindowFlag::Maximized));
    glfwWindowHint(GLFW_POSITION_X, new_pos_x);
    glfwWindowHint(GLFW_POSITION_Y, new_pos_y);

    auto impl = new Impl;
    impl->width = static_cast<u32>(new_width);
    impl->height = static_cast<u32>(new_height);
    impl->monitor_id = info.monitor;
    impl->handle = glfwCreateWindow(new_width, new_height, info.title.data(), nullptr, nullptr);

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

        std::vector<fs::path> paths(path_count);
        for (i32 i = 0; i < path_count; i++) {
            paths[i] = fs::path(paths_cstr[i]);
        }

        Application::get().push_event(
            ApplicationEvent::Drop,
            {
                .paths = std::move(paths),
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
    glm::ivec2 position = {};
    glfwGetMonitorPos(monitor, &position.x, &position.y);

    glm::ivec4 work_area = {};
    glfwGetMonitorWorkarea(monitor, &work_area.x, &work_area.y, &work_area.z, &work_area.w);

    return SystemDisplay{
        .name = monitor_name,
        .position = position,
        .work_area = work_area,
        .resolution = { vid_mode->width, vid_mode->height },
        .refresh_rate = vid_mode->refreshRate,
    };
}

auto Window::should_close() -> bool {
    return glfwWindowShouldClose(impl->handle);
}

auto Window::get_size() -> glm::uvec2 {
    return { impl->width, impl->height };
}

auto Window::get_native_handle() -> void * {
#if defined(LS_WINDOWS)
    return reinterpret_cast<void *>(glfwGetWin32Window(impl->handle));
#elif defined(LS_LINUX)
    return reinterpret_cast<void *>(glfwGetX11Window(impl->handle));
#endif
}

auto vk::get_vk_surface(VkInstance instance, void *handle) -> std::expected<VkSurfaceKHR, vk::Result> {
    ZoneScoped;

    VkSurfaceKHR surface = {};

#if defined(LS_WINDOWS)
    auto vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

    VkWin32SurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .hinstance = GetModuleHandleA(nullptr),
        .hwnd = reinterpret_cast<HWND>(handle),
    };

    auto result = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface);
#elif defined(LS_LINUX)
    auto vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");

    VkXlibSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .dpy = glfwGetX11Display(),
        .window = reinterpret_cast<::Window>(handle),
    };

    auto result = vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface);
#endif
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    return surface;
}

}  // namespace lr
