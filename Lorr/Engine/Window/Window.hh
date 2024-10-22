#pragma once

#include "Engine/Core/Handle.hh"
#include "Engine/Graphics/Vulkan.hh"

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
consteval void enable_bitmask(WindowFlag);

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

struct Window : Handle<Window> {
    static auto create(const WindowInfo &info) -> Window;
    auto destroy() -> void;

    auto poll() -> void;
    auto set_cursor(WindowCursor cursor) -> void;

    static auto display_at(u32 monitor_id = WindowInfo::USE_PRIMARY_MONITOR) -> SystemDisplay;
    auto should_close() -> bool;
    auto get_size() -> glm::uvec2;
    auto get_native_handle() -> void *;
    auto get_surface(Device) -> Surface;
};

}  // namespace lr
