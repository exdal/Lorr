// Created on Wednesday May 4th 2022 by exdal
// Last modified on Tuesday August 29th 2023 by exdal

#pragma once

#include "Core/EventManager.hh"
#include "Input/Key.hh"
#include "UI/ImGui.hh"
#include "Window/Win32/Win32Window.hh"
#include "Renderer/Renderer.hh"

namespace lr
{
// Or OS level events
enum EngineEvent : Event
{
    ENGINE_EVENT_QUIT,
    ENGINE_EVENT_RESIZE,
    ENGINE_EVENT_MOUSE_POSITION,
    ENGINE_EVENT_MOUSE_STATE,
    ENGINE_EVENT_MOUSE_WHEEL,
    ENGINE_EVENT_KEYBOARD_STATE,
    ENGINE_EVENT_CURSOR_STATE,
};

union EngineEventData
{
    u32 __data[4] = {};

    struct  // ENGINE_EVENT_RESIZE
    {
        u32 m_size_width;
        u32 m_size_height;
    };

    struct  // ENGINE_EVENT_MOUSE_POSITION
    {
        u32 m_mouse_x;
        u32 m_mouse_y;
    };

    struct  // ENGINE_EVENT_MOUSE_STATE
    {
        Key m_mouse;
        MouseState m_mouse_state;
    };

    struct  // ENGINE_EVENT_MOUSE_WHEEL
    {
        float m_offset;
    };

    struct  // ENGINE_EVENT_KEYBOARD_STATE
    {
        Key m_key;
        KeyState m_key_state;
    };

    struct  // ENGINE_EVENT_CURSOR_STATE
    {
        WindowCursor m_window_cursor;
    };
};

struct EngineDesc
{
    WindowDesc m_window_desc;
};

struct Engine
{
    void create(EngineDesc &engineDesc);

    /// EVENTS ///
    void push_event(Event event, EngineEventData &data);
    void dispatch_events();

    void prepare();
    void begin_frame();
    void end_frame();

    EventManager<EngineEventData, 64> m_event_man;
    Win32Window m_window;
    UI::ImGuiHandler m_imgui;
    Renderer::Renderer m_renderer = {};

    bool m_shutting_down = false;

    static auto get_window() { return &get()->m_window; }
    static Engine *get();
};

}  // namespace lr