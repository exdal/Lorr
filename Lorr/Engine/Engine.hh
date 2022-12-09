//
// Created on Wednesday 4th May 2022 by e-erdal
//

#pragma once

#include "Core/Window/BaseWindow.hh"
#include "Core/Window/Win32/Win32Window.hh"

#include "Renderer/RendererManager.hh"
#include "UI/ImGui.hh"

namespace lr
{
    struct Engine
    {
        void Init(WindowDesc &windowDesc);

        /// EVENTS ///
        void DispatchEvents();

        void BeginFrame();
        void EndFrame();

        Win32Window m_Window;
        Renderer::RendererManager m_RendererMan;
        UI::ImGuiHandler m_ImGui;

        bool m_ShuttingDown = false;
    };

}  // namespace lr