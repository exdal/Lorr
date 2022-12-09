//
// Created on Tuesday 18th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseCommandQueue.hh"

#include "D3D12Sym.hh"

#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    struct D3D12CommandQueue : BaseCommandQueue
    {
        void ExecuteCommandList(D3D12CommandList *pList);
        void WaitIdle();

        ID3D12CommandQueue *m_pHandle = nullptr;
        
        ID3D12Fence *m_pFence = nullptr;  // Fence if we need to wait for queue itself (i.e SwapChain resize)
        u64 m_FenceValue = 0;
        HANDLE m_FenceEvent = nullptr;
    };

}  // namespace lr::Graphics