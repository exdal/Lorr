//
// Created on Friday 28th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseShader.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKShader : BaseShader
    {
        VkShaderModule pHandle;
    };

}  // namespace lr::Graphics