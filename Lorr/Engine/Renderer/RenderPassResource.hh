// Created on Tuesday August 22nd 2023 by exdal
// Last modified on Tuesday August 22nd 2023 by exdal

#pragma once

#include "Graphics/CommandList.hh"

namespace lr::Graphics
{
enum class ResourceType : u32
{
    Buffer,
    Image,
    Sampler,
};

template<typename _T>
struct ToResourceType
{
};

template<>
struct ToResourceType<Buffer>
{
    constexpr static ResourceType kType = ResourceType::Buffer;
};

template<>
struct ToResourceType<Image>
{
    constexpr static ResourceType kType = ResourceType::Image;
};

template<>
struct ToResourceType<Sampler>
{
    constexpr static ResourceType kType = ResourceType::Sampler;
};

// Only used to read it's memory from, do not use in RenderGraph
struct RenderPassResource
{
    ResourceType m_Type = ResourceType::Buffer;
    void *m_pHandle = nullptr;
    MemoryAccess m_SrcAccess = MemoryAccess::None;
    MemoryAccess m_DstAccess = MemoryAccess::None;
};

template<typename _Resource, MemoryAccess _DstAccess>
struct RenderPassResource_t
{
    constexpr RenderPassResource_t() = default;
    constexpr RenderPassResource_t(_Resource *pHandle, MemoryAccess dstAccess)
        : m_pHandle(pHandle),
          m_DstAccess(dstAccess)
    {
    }

    ResourceType m_Type = ToResourceType<_Resource>::kType;
    _Resource *m_pHandle = nullptr;
    MemoryAccess m_SrcAccess = MemoryAccess::None;  // Set on RenderGraph initialization
    MemoryAccess m_DstAccess = _DstAccess;
};

}  // namespace lr::Graphics