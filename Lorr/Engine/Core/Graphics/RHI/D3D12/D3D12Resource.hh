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

        D3D12_CPU_DESCRIPTOR_HANDLE m_ViewHandle = {};
    };

    struct D3D12DescriptorSet : BaseDescriptorSet
    {
        ID3D12RootSignature *m_pHandle = nullptr;
    };

}  // namespace lr::Graphics