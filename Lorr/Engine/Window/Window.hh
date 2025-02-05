#pragma once

#include "Engine/Core/Handle.hh"

#include <vulkan/vulkan_core.h>

#include <SDL3/SDL_keycode.h>

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

    Count,
};

enum class WindowFlag : u32 {
    None = 0,
    Centered = 1 << 0,
    Resizable = 1 << 1,
    Borderless = 1 << 2,
    Maximized = 1 << 3,
    WorkAreaRelative = 1 << 4,  // Width and height of the window will be relative to available work area size
};
consteval void enable_bitmask(WindowFlag);

struct SystemDisplay {
    std::string name = {};

    glm::ivec2 position = {};
    glm::ivec4 work_area = {};
    glm::ivec2 resolution = {};
    f32 refresh_rate = 30.0f;
};

struct WindowCallbacks {
    void *user_data = nullptr;
    void (*on_resize)(void *user_data, glm::uvec2 size) = nullptr;
    void (*on_mouse_pos)(void *user_data, glm::vec2 position, glm::vec2 relative) = nullptr;
    void (*on_mouse_button)(void *user_data, u8 button, bool down) = nullptr;
    void (*on_mouse_scroll)(void *user_data, glm::vec2 offset) = nullptr;
    void (*on_text_input)(void *user_data, const c8 *text) = nullptr;
    void (*on_key)(void *user_data, SDL_Keycode key_code, SDL_Scancode scan_code, u16 mods, bool down) = nullptr;
    void (*on_close)(void *user_data) = nullptr;
};

struct WindowInfo {
    constexpr static i32 USE_PRIMARY_MONITOR = 0;

    std::string title = {};
    std::string icon = {};
    i32 monitor = USE_PRIMARY_MONITOR;
    u32 width = 0;
    u32 height = 0;
    WindowFlag flags = WindowFlag::None;
};

struct Window : Handle<Window> {
    static auto create(const WindowInfo &info) -> Window;
    auto destroy() -> void;

    auto poll(const WindowCallbacks &callbacks) -> void;
    auto set_cursor(WindowCursor cursor) -> void;
    auto get_cursor() -> WindowCursor;
    auto show_cursor(bool show) -> void;

    static auto display_at(i32 monitor_id = WindowInfo::USE_PRIMARY_MONITOR) -> ls::option<SystemDisplay>;
    auto get_size() -> glm::uvec2;
    auto get_surface(VkInstance instance) -> VkSurfaceKHR;
};

}  // namespace lr
