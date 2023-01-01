//
// Created on Thursday 27th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BasePipeline.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12Pipeline : BasePipeline
    {
        ID3D12PipelineState *pHandle = nullptr;
        ID3D12RootSignature *pLayout = nullptr;

        u32 pRootConstats[LR_SHADER_STAGE_COUNT] = {};
    };

}  // namespace lr::Graphics