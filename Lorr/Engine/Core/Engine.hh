//
// Created on Wednesday 4th May 2022 by e-erdal
//

#pragma once

#include "Window/PlatformWindow.hh"

#include "Graphics/API/Vulkan/VKAPI.hh"

#include "OS/ThreadPool.hh"

namespace lr
{
    struct ApplicationDesc
    {
        bool ConsoleApp = false;
    };

    struct Engine
    {
        void Init(ApplicationDesc &desc, WindowDesc &windowDesc);

        void Run();

        PlatformWindow m_Window;

        g::VKAPI *m_pAPI = nullptr;

        ThreadPoolSPSC m_ThreadPoolSPSCAt;
    };

    extern Engine *GetEngine();

}  // namespace lr