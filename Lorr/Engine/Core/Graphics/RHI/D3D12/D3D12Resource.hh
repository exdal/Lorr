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

        D3D12_CPU_DESCRIPTOR_HANDLE m_ViewHandle = {};
    };

    struct D3D12Buffer : BaseBuffer
    {
        ID3D12Resource *m_pHandle = nullptr;
        ID3D12Heap *m_pMemoryHandle = nullptr;

        u64 m_Alignment = 0;

        D3D12_CPU_DESCRIPTOR_HANDLE m_ViewHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE m_DeviceHandle = {};
    };

    struct D3D12DescriptorSet : BaseDescriptorSet
    {
        ID3D12RootSignature *m_pHandle = nullptr;
        u32 m_DescriptorCount = 0;
        D3D12_GPU_VIRTUAL_ADDRESS m_pDescriptorHandle[8];
    };

}  // namespace lr::Graphics