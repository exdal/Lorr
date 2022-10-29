//
// Created on Tuesday 25th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Common.hh"

namespace lr::Graphics
{
    struct BaseShader
    {
        ShaderType Type = ShaderType::Count;
    };

}  // namespace lr::Graphics