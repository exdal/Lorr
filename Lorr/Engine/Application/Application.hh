//
// Created on Friday 9th December 2022 by exdal
//

#pragma once

#include "Engine.hh"

namespace lr
{
struct BaseApplicationDesc
{
    eastl::string_view m_name;

    EngineDesc m_engine_desc;
};

struct Application
{
    void create(BaseApplicationDesc &desc);

    virtual void shutdown() = 0;
    virtual void poll(f32 deltaTime) = 0;

    void run();

    Engine m_engine;
    eastl::string_view m_name;

    static Application *get();
};

}  // namespace lr