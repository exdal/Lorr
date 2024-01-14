#pragma once

#include "Core/EventManager.hh"
#include "Input/Key.hh"

namespace lr
{
enum class WindowCursor
{
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

enum class WindowFlag : u32
{
    None = 0,
    FullScreen = 1 << 0,
    Centered = 1 << 1,
    Resizable = 1 << 2,
    Borderless = 1 << 3,
    Maximized = 1 << 4,
};
LR_TYPEOP_ARITHMETIC(WindowFlag, WindowFlag, |);
LR_TYPEOP_ARITHMETIC_INT(WindowFlag, WindowFlag, &);

enum WindowEvent : Event
{
    LR_WINDOW_EVENT_QUIT,
    LR_WINDOW_EVENT_RESIZE,
    LR_WINDOW_EVENT_MOUSE_POSITION,
    LR_WINDOW_EVENT_MOUSE_STATE,
    LR_WINDOW_EVENT_MOUSE_SCROLL,
    LR_WINDOW_EVENT_KEYBOARD_STATE,
    LR_WINDOW_EVENT_CURSOR_STATE,
};

union WindowEventData
{
    u32 __data[4] = {};

    struct
    {
        u32 m_size_width;
        u32 m_size_height;
    };

    struct
    {
        u32 m_mouse_x;
        u32 m_mouse_y;
    };

    struct
    {
        Key m_mouse;
        MouseState m_mouse_state;
    };

    struct
    {
        float m_offset;
    };

    struct
    {
        Key m_key;
        KeyState m_key_state;
    };

    struct
    {
        WindowCursor m_window_cursor;
    };
};

using WindowEventManager = EventManager<WindowEventData, 64>;

struct SystemMetrics
{
    struct Display
    {
        eastl::string m_name;

        u32 m_res_w;
        u32 m_res_h;
        i32 m_pos_x;
        i32 m_pos_y;

        u32 m_refresh_rate;
    };

    u8 m_display_size = 0;

    static constexpr u32 k_max_supported_display = 4;
    eastl::array<Display, k_max_supported_display> m_displays = {};
};

struct WindowDesc
{
    eastl::string_view m_title = "";
    eastl::string_view m_icon = "";
    u32 m_current_monitor = 0;
    u32 m_width = 0;
    u32 m_height = 0;
    WindowFlag m_flags = WindowFlag::None;
};

struct BaseWindow
{
    virtual ~BaseWindow() = default;

    void push_event(Event event, WindowEventData &data);
    virtual void poll();

    virtual void init_displays() = 0;
    SystemMetrics::Display *get_display(u32 monitor);

    virtual void set_cursor(WindowCursor cursor) = 0;

    bool should_close();

    void *m_handle = nullptr;

    u32 m_width = 0;
    u32 m_height = 0;

    bool m_should_close = false;
    bool m_is_fullscreen = false;
    bool m_size_ended = true;

    WindowCursor m_current_cursor = WindowCursor::Arrow;
    glm::uvec2 m_cursor_position = {};
    WindowEventManager m_event_manager = {};

    SystemMetrics m_system_metrics = {};
    u32 m_using_monitor = 0;
};

}  // namespace lr