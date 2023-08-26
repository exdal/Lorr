// Created on Tuesday August 22nd 2023 by exdal
// Last modified on Tuesday August 22nd 2023 by exdal

#pragma once

#include "Graphics/CommandList.hh"

namespace lr::Graphics
{

template<typename _Resource, MemoryAccess _DstAccess>
struct RenderPassResource
{
    constexpr RenderPassResource() = default;
    constexpr RenderPassResource(_Resource *pHandle, MemoryAccess dstAccess)
        : m_pHandle(pHandle),
          m_DstAccess(dstAccess)
    {
    }

    _Resource *m_pHandle = nullptr;
    MemoryAccess m_SrcAccess = MemoryAccess::None;  // Set on RenderGraph initialization
    MemoryAccess m_DstAccess = _DstAccess;
};

}  // namespace lr::Graphics