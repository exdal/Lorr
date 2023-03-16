//
// Created on Thursday 26th January 2023 by exdal
//

#pragma once

#include "Lorr/Engine/Application/Application.hh"

using namespace lr;

struct GIApp : Application
{
    void Init(BaseApplicationDesc &desc);
    void Shutdown() override;
    void Poll(f32 deltaTime) override;
};

#define LR_APP_WORKING_DIR "../../Examples/GI/"
#define LR_MAKE_APP_PATH(path) LR_APP_WORKING_DIR path