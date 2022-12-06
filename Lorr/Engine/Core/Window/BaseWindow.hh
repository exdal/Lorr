//
// Created on Thursday 5th May 2022 by e-erdal
//

#pragma once

#include "Core/EventManager.hh"

#include "Core/Input/Key.hh"

namespace lr
{
    enum WindowEvent : Event
    {
        WINDOW_EVENT_QUIT,
        WINDOW_EVENT_RESIZE,
        WINDOW_EVENT_MOUSE_POSITION,
        WINDOW_EVENT_MOUSE_STATE,
        WINDOW_EVENT_MOUSE_WHEEL,
        WINDOW_EVENT_KEYBOARD_STATE,
    };

    union WindowEventData
    {
        u32 __data[4] = {};

        struct  // WINDOW_EVENT_RESIZE
        {
            u32 SizeWidth;
            u32 SizeHeight;
        };

        struct  // WINDOW_EVENT_MOUSE_POSITION
        {
            u32 MouseX;
            u32 MouseY;
        };

        struct  // WINDOW_EVENT_MOUSE_STATE
        {
            Input::Key Mouse;
            Input::MouseState MouseState;
        };

        struct  // WINDOW_EVENT_MOUSE_WHEEL
        {
            float Offset;
        };

        struct  // WINDOW_EVENT_KEYBOARD_STATE
        {
            Input::Key Key;
            Input::KeyState KeyState;
        };
    };

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

    struct BaseWindow
    {
        virtual void Poll() = 0;

        virtual void InitDisplays() = 0;
        SystemMetrics::Display *GetDisplay(u32 monitor);

        virtual void SetCursor(Cursor cursor) = 0;

        bool ShouldClose();
        void OnSizeChanged(u32 width, u32 height);

        void *m_pHandle = nullptr;

        u32 m_Width = 0;
        u32 m_Height = 0;

        bool m_ShouldClose = false;
        bool m_IsFullscreen = false;
        bool m_SizeEnded = true;

        Cursor m_CurrentCursor = Cursor::Arrow;
        XMUINT2 m_CursorPosition = XMUINT2(0, 0);

        SystemMetrics m_SystemMetrics;
        u32 m_UsingMonitor = 0;

        EventManager<WindowEventData> m_EventManager;
    };

}  // namespace lr