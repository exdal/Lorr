#pragma once

#include "Lorr/Engine/Application/Application.hh"

struct TriangleApp : lr::Application
{
    void Init(lr::BaseApplicationDesc &desc);
    void Shutdown() override;
    void Poll(f32 deltaTime) override;
};