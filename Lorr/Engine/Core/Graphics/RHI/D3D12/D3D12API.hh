//
// Created on Tuesday 18th October 2022 by e-erdal
//

#pragma once

#include <eathread/eathread_thread.h>

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Common.hh"

#include "Core/IO/BufferStream.hh"
#include "Core/IO/Memory.hh"
#include "Core/IO/MemoryAllocator.hh"

#include "D3D12Sym.hh"

#include "D3D12SwapChain.hh"
#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    struct D3D12API
    {
        bool Init(PlatformWindow *pWindow, u32 width, u32 height, APIFlags flags);

        void InitCommandLists();
        void InitAllocators();

        /// COMMAND ///
        void CreateCommandQueue(D3D12CommandQueue *pQueue, CommandListType type);
        void CreateCommandAllocator(D3D12CommandAllocator *pAllocator, CommandListType type);
        void CreateCommandList(D3D12CommandList *pList, CommandListType type);

        D3D12CommandList *GetCommandList();

        void BeginCommandList(D3D12CommandList *pList);
        void EndCommandList(D3D12CommandList *pList);
        void ResetCommandAllocator(D3D12CommandAllocator *pAllocator);

        // Executes a specific command list, taken from a `CommandListPool`.
        // Does not execute command list in flight. Cannot perform a present operation.
        // if `waitForFence` set true, does not push fence into wait thread, blocks current thread.
        void ExecuteCommandList(D3D12CommandList *pList, bool waitForFence);

        /// API Handles
        ID3D12Device *m_pDevice = nullptr;
        IDXGIFactory4 *m_pFactory = nullptr;
        IDXGIAdapter1 *m_pAdapter = nullptr;

        D3D12CommandQueue m_DirectQueue;

        static constexpr D3D_FEATURE_LEVEL kMinimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
    };

}  // namespace lr::Graphics