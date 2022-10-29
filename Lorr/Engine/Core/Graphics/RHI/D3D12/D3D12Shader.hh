//
// Created on Friday 28th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseShader.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12Shader : BaseShader
    {
        D3D12_SHADER_BYTECODE pHandle;
    };

}  // namespace lr::Graphics