// Created on Wednesday May 17th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#pragma once

#include "Lorr/Engine/Application/Application.hh"

struct LorrApp : lr::Application
{
    void Init(lr::BaseApplicationDesc &desc);
    void Shutdown() override;
    void Poll(f32 deltaTime) override;
};