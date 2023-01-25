//
// Created on Friday 9th December 2022 by e-erdal
//

#pragma once

#include "Engine.hh"

namespace lr
{
    struct BaseApplicationDesc
    {
        eastl::string_view m_Name;
        eastl::string_view m_WorkingDir;

        EngineDesc m_EngineDesc;
    };

    struct Application
    {
        void InitBase(BaseApplicationDesc &desc);
        
        virtual void Shutdown() = 0;
        virtual void Poll(f32 deltaTime) = 0;

        void Run();

        Engine m_Engine;
        eastl::string_view m_Name;
        eastl::string_view m_WorkingDir;

        static Application *Get();
    };

}  // namespace lr