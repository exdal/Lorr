//
// Created on Friday 9th December 2022 by e-erdal
//

#pragma once

#include "Engine.hh"

namespace lr
{
    struct ApplicationDesc
    {
        eastl::string_view Title;
        EngineDesc Engine;
    };

    struct Application
    {
        virtual void Init(ApplicationDesc &desc) = 0;
        virtual void Shutdown() = 0;
        virtual void Poll(f32 deltaTime) = 0;

        void Run();

        Engine m_Engine;
        eastl::string_view m_Title;
    };

    Application *GetApp();

}  // namespace lr