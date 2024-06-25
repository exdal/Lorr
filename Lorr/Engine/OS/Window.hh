#pragma once

#include "Key.hh"
#include "Engine/Graphics/Vulkan.hh"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace lr {
enum class WindowCursor {
    Arrow,
    TextInput,
    ResizeAll,
    ResizeNS,
    ResizeEW,
    ResizeNESW,
    ResizeNWSE,
    Hand,
    NotAllowed,
    Hidden,

    Count,
};

enum class WindowFlag : u32 {
    None = 0,
    Centered = 1 << 0,
    Resizable = 1 << 1,
    Borderless = 1 << 2,
    Maximized = 1 << 3,
};
template<>
struct has_bitmask<WindowFlag> : std::true_type {};

struct SystemDisplay {
    std::string name;

    i32 res_w;
    i32 res_h;
    i32 pos_x;
    i32 pos_y;

    i32 refresh_rate;
};

struct WindowInfo {
    constexpr static u32 USE_PRIMARY_MONITOR = ~0_u32;

    std::string_view title = {};
    std::string_view icon = {};
    u32 monitor = USE_PRIMARY_MONITOR;
    u32 width = 0;
    u32 height = 0;
    WindowFlag flags = WindowFlag::None;
};

struct Window {
    bool init(this Window &, const WindowInfo &info);
    void poll(this Window &);
    void set_cursor(this Window &, WindowCursor cursor);

    SystemDisplay display_at(this Window &, u32 monitor_id = WindowInfo::USE_PRIMARY_MONITOR);
    Result<VkSurfaceKHR, VkResult> get_surface(this Window &, VkInstance instance);
    bool should_close(this Window &);

    u32 width = 0;
    u32 height = 0;

    WindowCursor current_cursor = WindowCursor::Arrow;
    glm::uvec2 cursor_position = {};

    GLFWwindow *handle = nullptr;
    u32 monitor_id = WindowInfo::USE_PRIMARY_MONITOR;
    std::array<GLFWcursor *, usize(WindowCursor::Count)> cursors = {};
};

}  // namespace lr
