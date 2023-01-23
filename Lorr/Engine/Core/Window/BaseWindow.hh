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
            u32 m_SizeWidth;
            u32 m_SizeHeight;
        };

        struct  // WINDOW_EVENT_MOUSE_POSITION
        {
            u32 m_MouseX;
            u32 m_MouseY;
        };

        struct  // WINDOW_EVENT_MOUSE_STATE
        {
            Key m_Mouse;
            MouseState m_MouseState;
        };

        struct  // WINDOW_EVENT_MOUSE_WHEEL
        {
            float m_Offset;
        };

        struct  // WINDOW_EVENT_KEYBOARD_STATE
        {
            Key m_Key;
            KeyState m_KeyState;
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
            eastl::string m_Name;

            u32 m_ResW;
            u32 m_ResH;
            u32 m_PosX;
            u32 m_PosY;

            u32 m_RefreshRate;
        };

        u8 m_DisplaySize = 0;

        static constexpr u32 kMaxSupportedDisplay = 4;
        eastl::array<Display, kMaxSupportedDisplay> m_Displays;
    };

    struct WindowDesc
    {
        eastl::string_view m_Title = "";
        eastl::string_view m_Icon = "";

        u32 m_CurrentMonitor = 0;

        u32 m_Width;
        u32 m_Height;

        WindowFlags m_Flags;
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