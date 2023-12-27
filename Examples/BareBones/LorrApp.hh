// Created on Wednesday May 17th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#pragma once

#include "Lorr/Engine/Application/Application.hh"

struct LorrApp : lr::Application
{
    void init(lr::BaseApplicationDesc &desc);
    void shutdown() override;
    void poll(f32 deltaTime) override;
};