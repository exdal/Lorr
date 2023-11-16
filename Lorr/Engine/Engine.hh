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
        u32 m_SizeWidth;
        u32 m_SizeHeight;
    };

    struct  // ENGINE_EVENT_MOUSE_POSITION
    {
        u32 m_MouseX;
        u32 m_MouseY;
    };

    struct  // ENGINE_EVENT_MOUSE_STATE
    {
        Key m_Mouse;
        MouseState m_MouseState;
    };

    struct  // ENGINE_EVENT_MOUSE_WHEEL
    {
        float m_Offset;
    };

    struct  // ENGINE_EVENT_KEYBOARD_STATE
    {
        Key m_Key;
        KeyState m_KeyState;
    };

    struct  // ENGINE_EVENT_CURSOR_STATE
    {
        WindowCursor m_WindowCursor;
    };
};

struct EngineDesc
{
    WindowDesc m_WindowDesc;
};

struct Engine
{
    void Init(EngineDesc &engineDesc);

    /// EVENTS ///
    void PushEvent(Event event, EngineEventData &data);
    void DispatchEvents();

    void Prepare();
    void BeginFrame();
    void EndFrame();

    EventManager<EngineEventData, 64> m_EventMan;
    Win32Window m_Window;
    UI::ImGuiHandler m_ImGui;
    Renderer::Renderer m_Renderer = {};

    bool m_ShuttingDown = false;

    static auto GetWindow() { return &Get()->m_Window; }
    // static auto GetRenderGraph() { return &Get()->m_RenderGraph; }
    static Engine *Get();
};

}  // namespace lr