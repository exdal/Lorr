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
        eastl::atomic<u64> DesiredFenceValue = 0;
        ID3D12Fence *pFence = nullptr;
        HANDLE FenceEvent = nullptr;

        ID3D12CommandAllocator *pHandle = nullptr;
    };

    struct D3D12CommandList : BaseCommandList
    {
        void Init(ID3D12GraphicsCommandList4 *pHandle, D3D12CommandAllocator *pAllocator, CommandListType type);

        CommandListType m_Type;
        D3D12CommandAllocator m_Allocator;
        ID3D12GraphicsCommandList4 *m_pHandle = nullptr;
    };

}  // namespace lr::Graphics