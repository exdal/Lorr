//
// Created on Thursday 5th May 2022 by exdal
//

#pragma once

#include "Core/EventManager.hh"

#include "Input/Key.hh"

namespace lr
{
    enum WindowCursor : u8
    {
        LR_WINDOW_CURSOR_ARROW,
        LR_WINDOW_CURSOR_TEXT_INPUT,
        LR_WINDOW_CURSOR_RESIZE_ALL,
        LR_WINDOW_CURSOR_RESIZE_NS,
        LR_WINDOW_CURSOR_RESIZE_EW,
        LR_WINDOW_CURSOR_RESIZE_NESW,
        LR_WINDOW_CURSOR_RESIZE_NWSE,
        LR_WINDOW_CURSOR_HAND,
        LR_WINDOW_CURSOR_NOT_ALLOWED,
        LR_WINDOW_CURSOR_HIDDEN,
    };

    enum WindowFlags : u8
    {
        LR_WINDOW_FLAG_NONE,
        LR_WINDOW_FLAG_FULLSCREEN = 1 << 0,
        LR_WINDOW_FLAG_CENTERED = 1 << 1,
        LR_WINDOW_FLAG_RESIZABLE = 1 << 2,
        LR_WINDOW_FLAG_BORDERLESS = 1 << 3,
        LR_WINDOW_FLAG_MAXIMIZED = 1 << 4,
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

        virtual void SetCursor(WindowCursor cursor) = 0;

        bool ShouldClose();
        void OnSizeChanged(u32 width, u32 height);

        void *m_pHandle = nullptr;

        u32 m_Width = 0;
        u32 m_Height = 0;

        bool m_ShouldClose = false;
        bool m_IsFullscreen = false;
        bool m_SizeEnded = true;

        WindowCursor m_CurrentCursor = LR_WINDOW_CURSOR_ARROW;
        XMUINT2 m_CursorPosition = XMUINT2(0, 0);

        SystemMetrics m_SystemMetrics;
        u32 m_UsingMonitor = 0;
    };

}  // namespace lr