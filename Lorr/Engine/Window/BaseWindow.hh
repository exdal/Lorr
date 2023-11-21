#pragma once

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
    WindowFlag m_Flags = WindowFlag::None;
};

struct BaseWindow
{
    virtual void poll() = 0;

    virtual void init_displays() = 0;
    SystemMetrics::Display *get_display(u32 monitor);

    virtual void set_cursor(WindowCursor cursor) = 0;

    bool should_close();
    void on_size_changed(u32 width, u32 height);

    void *m_handle = nullptr;

    u32 m_width = 0;
    u32 m_height = 0;

    bool m_should_close = false;
    bool m_is_fullscreen = false;
    bool m_size_ended = true;

    WindowCursor m_current_cursor = WindowCursor::Arrow;
    XMUINT2 m_cursor_position = XMUINT2(0, 0);

    SystemMetrics m_system_metrics = {};
    u32 m_using_monitor = 0;
};

}  // namespace lr