//
// Created on Friday 9th December 2022 by exdal
//

#pragma once

#include "Engine.hh"

namespace lr
{
    struct BaseApplicationDesc
    {
        eastl::string_view m_Name;

        EngineDesc m_EngineDesc;
    };

    struct Application
    {
        void PreInit(BaseApplicationDesc &desc);
        
        virtual void Shutdown() = 0;
        virtual void Poll(f32 deltaTime) = 0;

        void Run();

        Engine m_Engine;
        eastl::string_view m_Name;

        static Application *Get();
    };

}  // namespace lr