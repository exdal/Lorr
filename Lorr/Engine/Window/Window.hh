#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <vuk/runtime/vk/VkSwapchain.hpp>

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
    WorkAreaRelative = 1 << 4, // Width and height of the window will be relative to available work area size
    Fullscreen = 1 << 5,
};
consteval void enable_bitmask(WindowFlag);

struct SystemDisplay {
    std::string name = {};

    glm::ivec2 position = {};
    glm::ivec4 work_area = {};
    glm::ivec2 resolution = {};
    f32 refresh_rate = 30.0f;
};

enum class DialogKind : u32 {
    OpenFile = 0,
    SaveFile,
    OpenFolder,
};

struct FileDialogFilter {
    std::string_view name = {};
    std::string_view pattern = {};
};

struct ShowDialogInfo {
    DialogKind kind = DialogKind::OpenFile;
    void *user_data = nullptr;
    std::string_view title = {};
    fs::path spawn_path = {};
    ls::span<FileDialogFilter> filters = {};
    bool multi_select = false;
    void (*callback)(void *user_data, const c8 *const *files, i32 filter) = nullptr;
};

struct WindowInfo {
    std::string title = {};
    SystemDisplay *display = nullptr;
    i32 width = 0;
    i32 height = 0;
    WindowFlag flags = WindowFlag::None;
};

struct Window {
    constexpr static auto MODULE_NAME = "Window";

    std::string title = {};
    i32 width = 0;
    i32 height = 0;
    WindowFlag flags = WindowFlag::None;
    SystemDisplay *display = nullptr;

    SDL_Window *handle = nullptr;
    ls::option<vuk::Swapchain> swap_chain = ls::nullopt;

    WindowCursor current_cursor = WindowCursor::Arrow;
    glm::uvec2 cursor_position = {};
    std::array<SDL_Cursor *, usize(WindowCursor::Count)> cursors = {};
    std::vector<std::function<void(SDL_Event &)>> event_listeners = {};

    static auto init_sdl() -> bool;
    static auto display_at(i32 monitor_id) -> ls::option<SystemDisplay>;

    Window(const WindowInfo &info);
    auto init(this Window &) -> bool;
    auto destroy(this Window &) -> void;
    auto update(this Window &, f64) -> void;

    auto set_cursor(this Window &, WindowCursor cursor) -> void;
    auto get_cursor(this Window &) -> WindowCursor;
    auto show_cursor(this Window &, bool show) -> void;

    template<typename T>
    auto add_listener(T &listener) {
        event_listeners.push_back([&listener](SDL_Event &e) { listener.window_event(e); });
    }

    auto get_size(this Window &) -> glm::ivec2;
    auto get_surface(this Window &, VkInstance instance) -> VkSurfaceKHR;
    auto get_handle(this Window &) -> void *;

    auto show_dialog(this Window &, const ShowDialogInfo &info) -> void;
};

} // namespace lr
