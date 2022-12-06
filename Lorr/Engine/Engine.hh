//
// Created on Wednesday 4th May 2022 by e-erdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"
#include "Core/Window/Win32/Win32Window.hh"

#include "Renderer/RendererManager.hh"
#include "UI/ImGuiRenderer.hh"

namespace lr
{
    struct ApplicationDesc
    {
        bool ConsoleApp = false;
    };

    struct Engine
    {
        void Init(ApplicationDesc &desc, WindowDesc &windowDesc);

        /// EVENTS ///
        void Poll(f32 deltaTime);

        void Run();

        Win32Window m_Window;
        Renderer::RendererManager m_RendererMan;
        UI::ImGuiRenderer m_ImGui;

        bool m_ShuttingDown = false;
    };

    extern Engine *GetEngine();

}  // namespace lr