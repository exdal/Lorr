//
// Created on Friday 9th December 2022 by e-erdal
//

#pragma once

#include "Lorr/Engine/Application/Application.hh"

struct ImguiApp : lr::Application
{
    void Init(lr::BaseApplicationDesc &desc);
    void Shutdown() override;
    void Poll(f32 deltaTime) override;
};