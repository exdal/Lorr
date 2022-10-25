//
// Created on Wednesday 19th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12CommandAllocator
    {
        void Reset();

        eastl::atomic<u64> m_FenceValue = 0;
        ID3D12Fence *m_pFence = nullptr;
        HANDLE m_FenceEvent = nullptr;

        ID3D12CommandAllocator *m_pHandle = nullptr;
    };

    struct D3D12CommandList
    {
        void Reset();
        void Close();

        CommandListType m_Type;
        D3D12CommandAllocator m_Allocator;
        ID3D12GraphicsCommandList4 *m_pHandle = nullptr;
    };

}  // namespace lr::Graphics