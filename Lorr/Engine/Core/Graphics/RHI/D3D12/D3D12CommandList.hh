//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseCommandList.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12CommandAllocator : BaseCommandAllocator
    {
        void Reset()
        {
            pHandle->Reset();
        }

        ID3D12CommandAllocator *pHandle = nullptr;
    };

    struct D3D12CommandList : BaseCommandList
    {
        void Init(ID3D12GraphicsCommandList4 *pHandle, D3D12CommandAllocator *pAllocator, CommandListType type);

        void Reset(D3D12CommandAllocator *pAllocator);

        void BarrierTransition(BaseImage *pImage,
                               ResourceUsage barrierBefore,
                               ShaderStage shaderBefore,
                               ResourceUsage barrierAfter,
                               ShaderStage shaderAfter);

        D3D12CommandAllocator *m_pAllocator = nullptr;
        ID3D12GraphicsCommandList4 *m_pHandle = nullptr;

        eastl::atomic<u64> m_DesiredFenceValue = 0;
        ID3D12Fence *m_pFence = nullptr;
        HANDLE m_FenceEvent = nullptr;
    };

}  // namespace lr::Graphics