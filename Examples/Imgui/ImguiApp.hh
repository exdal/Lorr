//
// Created on Friday 9th December 2022 by exdal
//

#pragma once

#include "Lorr/Engine/Application/Application.hh"

struct ImguiApp : lr::Application
{
    void init(lr::BaseApplicationDesc &desc);
    void shutdown() override;
    void poll(f32 deltaTime) override;
};