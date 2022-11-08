//
// Created on Tuesday 25th October 2022 by e-erdal
//

#pragma once

namespace lr::Graphics
{
    enum class ShaderStage
    {
        None = 0,
        Vertex = 1 << 0,
        Pixel = 1 << 1,
        Compute = 1 << 2,
        Hull = 1 << 3,
        Domain = 1 << 4,
        Geometry = 1 << 5,
    };

    EnumFlags(ShaderStage);

    struct BaseShader
    {
        ShaderStage Type;
    };

}  // namespace lr::Graphics