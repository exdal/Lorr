//
// Created on Wednesday 4th May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Vulkan/VKAPI.hh"
#include "Core/Graphics/Camera.hh"
#include "Core/Window/BaseWindow.hh"

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
        void OnWindowResize(u32 width, u32 height);
        
        void Run();

        PlatformWindow m_Window;

        Graphics::VKAPI *m_pAPI = nullptr;

        Graphics::Camera3D m_Camera3D;
        Graphics::Camera2D m_Camera2D;
    };

    extern Engine *GetEngine();

}  // namespace lr