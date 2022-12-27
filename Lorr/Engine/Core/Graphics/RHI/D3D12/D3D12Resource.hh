//
// Created on Thursday 27th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseResource.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12Image : BaseImage
    {
        ID3D12Resource *m_pHandle = nullptr;
        ID3D12Heap *m_pMemoryHandle = nullptr;

        D3D12_CPU_DESCRIPTOR_HANDLE m_ShaderViewCPU = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_ShaderViewGPU = { 0 };

        D3D12_CPU_DESCRIPTOR_HANDLE m_UnorderedAccessViewCPU = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_UnorderedAccessViewGPU = { 0 };

        D3D12_CPU_DESCRIPTOR_HANDLE m_RenderTargetViewCPU = { 0 };

        D3D12_CPU_DESCRIPTOR_HANDLE m_DepthStencilViewCPU = { 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE m_DepthStencilViewGPU = { 0 };
    };

    struct D3D12Buffer : BaseBuffer
    {
        ID3D12Resource *m_pHandle = nullptr;
        ID3D12Heap *m_pMemoryHandle = nullptr;

        u64 m_Alignment = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE m_ViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_ViewGPU = {};
        D3D12_GPU_VIRTUAL_ADDRESS m_VirtualAddress = 0;
    };

    struct D3D12DescriptorSet : BaseDescriptorSet
    {
        ID3D12RootSignature *pHandle = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE pDescriptorHandles[8];
    };

}  // namespace lr::Graphics