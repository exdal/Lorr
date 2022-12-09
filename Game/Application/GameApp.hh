//
// Created on Friday 9th December 2022 by e-erdal
//

#pragma once

#include "Lorr/Engine/Application/Application.hh"

struct GameApp : lr::Application
{
    void Init(lr::ApplicationDesc &desc) override;
    void Shutdown() override;
    void Poll(f32 deltaTime) override;
};