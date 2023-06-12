// Created on Thursday May 5th 2022 by exdal
// Last modified on Monday June 12th 2023 by exdal

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

    EnumFlags(WindowFlag);

    struct SystemMetrics
    {
        struct Display
        {
            eastl::string m_Name;

            u32 m_ResW;
            u32 m_ResH;
            i32 m_PosX;
            i32 m_PosY;

            u32 m_RefreshRate;
        };

        u8 m_DisplaySize = 0;

        static constexpr u32 kMaxSupportedDisplay = 4;
        eastl::array<Display, kMaxSupportedDisplay> m_Displays = {};
    };

    struct WindowDesc
    {
        eastl::string_view m_Title = "";
        eastl::string_view m_Icon = "";

        u32 m_CurrentMonitor = 0;

        u32 m_Width;
        u32 m_Height;

        WindowFlag m_Flags = WindowFlag::None;
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

        WindowCursor m_CurrentCursor = WindowCursor::Arrow;
        XMUINT2 m_CursorPosition = XMUINT2(0, 0);

        SystemMetrics m_SystemMetrics;
        u32 m_UsingMonitor = 0;
    };

}  // namespace lr