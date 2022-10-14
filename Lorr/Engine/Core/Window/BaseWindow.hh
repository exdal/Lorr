//
// Created on Thursday 5th May 2022 by e-erdal
//

#pragma once

namespace lr
{
    enum class Cursor
    {
        Arrow,
        TextInput,
        ResizeAll,
        ResizeEW,
        ResizeNS,
        ResizeNESW,
        ResizeNWSE,
        Hand,
        NotAllowed,
        Hidden,
    };

    enum class WindowFlags : u8
    {
        None,
        Fullscreen = 1 << 0,
        VSync = 1 << 1,
        Centered = 1 << 2,  // Desktop dist. specific
        Resizable = 1 << 3,
        Borderless = 1 << 4,

        Maximized = 1 << 5,
    };

    EnumFlags(WindowFlags);

    struct SystemMetrics
    {
        struct Display
        {
            eastl::string Name;

            u32 ResW;
            u32 ResH;
            u32 PosX;
            u32 PosY;

            u32 RefreshRate;
        };

        u8 DisplaySize = 0;

        static constexpr u32 kMaxSupportedDisplay = 4;
        eastl::array<Display, kMaxSupportedDisplay> Displays;
    };

    struct WindowDesc
    {
        eastl::string_view Title = "";
        eastl::string_view Icon = "";

        u32 CurrentMonitor = 0;

        u32 Width;
        u32 Height;

        WindowFlags Flags;
    };

#if _WIN32
    struct Win32Window;
    using WindowHandle = HWND;
    using PlatformWindow = Win32Window;
#endif

    struct BaseWindow
    {
        void Init(const WindowDesc &desc);
        virtual void Poll() = 0;

        SystemMetrics::Display *GetDisplay(u32 monitor);

        virtual void SetCursor(Cursor cursor) = 0;

        bool ShouldClose();
        void OnSizeChanged(u32 width, u32 height);

        // This should not be confused with InputManager::GetMousePos
        // This function gets coordinates of monitor space mouse
        // Meanwhile InputManager gets window space mouse coordinates
        XMINT2 GetCursorPos()
        {
            ZoneScoped;

            LPPOINT point;
            ::GetCursorPos(point);

            return XMINT2(point->x, point->y);
        }

        virtual void NativeInit(const WindowDesc &desc) = 0;
        virtual void GetDisplays() = 0;

        WindowHandle m_Handle = nullptr;

        u32 m_Width = 0;
        u32 m_Height = 0;

        bool m_ShouldClose = false;
        bool m_IsFullscreen = false;
        bool m_SizeEnded = true;

        Cursor m_CurrentCursor = Cursor::Arrow;

        SystemMetrics m_SystemMetrics;
        u32 m_UsingMonitor = 0;
    };

}  // namespace lr