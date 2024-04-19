#pragma once

#include "Core/EventManager.hh"
#include "Graphics/Vulkan.hh"
#include "Input/Key.hh"

namespace lr::window {
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
};

enum class WindowFlag : u32 {
    None = 0,
    Centered = 1 << 0,
    Resizable = 1 << 1,
    Borderless = 1 << 2,
    Maximized = 1 << 3,
};
LR_TYPEOP_ARITHMETIC(WindowFlag, WindowFlag, |);
LR_TYPEOP_ARITHMETIC_INT(WindowFlag, WindowFlag, &);

enum WindowEvent : Event {
    LR_WINDOW_EVENT_INIT,
    LR_WINDOW_EVENT_QUIT,
    LR_WINDOW_EVENT_RESIZE,
    LR_WINDOW_EVENT_MOUSE_POSITION,
    LR_WINDOW_EVENT_MOUSE_STATE,
    LR_WINDOW_EVENT_MOUSE_SCROLL,
    LR_WINDOW_EVENT_KEYBOARD_STATE,
    LR_WINDOW_EVENT_CURSOR_STATE,
};

union WindowEventData {
    u32 reserved_data[4] = {};

    struct {
        u32 size_width;
        u32 size_height;
    };

    struct {
        u32 mouse_x;
        u32 mouse_y;
    };

    struct {
        Key mouse;
        MouseState mouse_state;
    };

    struct {
        float offset;
    };

    struct {
        Key key;
        KeyState key_state;
    };

    struct {
        WindowCursor window_cursor;
    };
};

using WindowEventManager = EventManager<WindowEventData, 64>;

struct SystemDisplay {
    std::string name;

    u32 res_w;
    u32 res_h;
    i32 pos_x;
    i32 pos_y;

    u32 refresh_rate;
};

struct WindowInfo {
    std::string_view title = {};
    std::string_view icon = {};
    i32 current_monitor = 0;
    i32 width = 0;
    i32 height = 0;
    WindowFlag flags = WindowFlag::None;
};

struct Window {
    virtual ~Window() = default;

    virtual bool init(const WindowInfo &info) = 0;
    virtual void poll() = 0;
    virtual void set_cursor(WindowCursor cursor) = 0;

    SystemDisplay *get_display(u32 monitor);
    bool should_close();

    virtual VkSurfaceKHR get_surface(VkInstance instance) = 0;

    void *m_handle = nullptr;

    u32 m_width = 0;
    u32 m_height = 0;

    bool m_should_close = false;

    WindowCursor m_current_cursor = WindowCursor::Arrow;
    glm::uvec2 m_cursor_position = {};
    WindowEventManager m_event_manager = {};

    static constexpr u32 MAX_DISPLAYS = 4;
    ls::static_vector<SystemDisplay, MAX_DISPLAYS> m_displays = {};
    u32 m_using_display = 0;
};

}  // namespace lr::window
