//
// Created on Thursday 27th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BasePipeline.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12Pipeline : Pipeline
    {
        ID3D12PipelineState *m_pHandle = nullptr;
        ID3D12RootSignature *m_pLayout = nullptr;

        u32 m_pRootConstats[LR_SHADER_STAGE_COUNT] = {};
    };

}  // namespace lr::Graphics