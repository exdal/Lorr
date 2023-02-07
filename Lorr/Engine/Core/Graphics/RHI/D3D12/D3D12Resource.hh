//
// Created on Thursday 27th October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseResource.hh"

#include "D3D12Sym.hh"

namespace lr::Graphics
{
    struct D3D12Image : Image
    {
        ID3D12Resource *m_pHandle = nullptr;
        ID3D12Heap *m_pMemoryHandle = nullptr;

        D3D12_CPU_DESCRIPTOR_HANDLE m_ShaderViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_ShaderViewGPU = {};

        D3D12_CPU_DESCRIPTOR_HANDLE m_UnorderedAccessViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_UnorderedAccessViewGPU = {};

        D3D12_CPU_DESCRIPTOR_HANDLE m_RenderTargetViewCPU = {};

        D3D12_CPU_DESCRIPTOR_HANDLE m_DepthStencilViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_DepthStencilViewGPU = {};
    };

    struct D3D12Buffer : Buffer
    {
        ID3D12Resource *m_pHandle = nullptr;
        ID3D12Heap *m_pMemoryHandle = nullptr;

        u64 m_Alignment = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE m_ConstantBufferViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_ConstantBufferViewGPU = {};

        D3D12_CPU_DESCRIPTOR_HANDLE m_ShaderViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_ShaderViewGPU = {};

        D3D12_CPU_DESCRIPTOR_HANDLE m_UnorderedAccessViewCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_UnorderedAccessViewGPU = {};

        D3D12_GPU_VIRTUAL_ADDRESS m_VirtualAddress = 0;
    };

    struct D3D12Sampler : Sampler
    {
        D3D12_CPU_DESCRIPTOR_HANDLE m_HandleCPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_HandleGPU = {};
    };

    struct D3D12DescriptorSet : DescriptorSet
    {
        ID3D12RootSignature *m_pHandle = nullptr;
        D3D12_GPU_DESCRIPTOR_HANDLE m_pDescriptorHandles[8];
    };

}  // namespace lr::Graphics