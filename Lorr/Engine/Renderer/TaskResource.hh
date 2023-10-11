#pragma once

#include "Graphics/Resource.hh"

namespace lr::Renderer
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
struct ToResourceType<Graphics::Buffer>
{
    constexpr static ResourceType kType = ResourceType::Buffer;
};

template<>
struct ToResourceType<Graphics::Image>
{
    constexpr static ResourceType kType = ResourceType::Image;
};

template<>
struct ToResourceType<Graphics::Sampler>
{
    constexpr static ResourceType kType = ResourceType::Sampler;
};

// Only used to read it's memory from, do not use in RenderGraph
struct RenderPassResource
{
    ResourceType m_Type = ResourceType::Buffer;
    void *m_pHandle = nullptr;
    Graphics::MemoryAccess m_SrcAccess = Graphics::MemoryAccess::None;
    Graphics::MemoryAccess m_DstAccess = Graphics::MemoryAccess::None;
};

template<typename _Resource, Graphics::MemoryAccess _DstAccess>
struct RenderPassResource_t
{
    constexpr RenderPassResource_t() = default;
    constexpr RenderPassResource_t(_Resource *pHandle, Graphics::MemoryAccess dstAccess)
        : m_pHandle(pHandle),
          m_DstAccess(dstAccess)
    {
    }

    ResourceType m_Type = ToResourceType<_Resource>::kType;
    _Resource *m_pHandle = nullptr;
    Graphics::MemoryAccess m_SrcAccess = Graphics::MemoryAccess::None;
    Graphics::MemoryAccess m_DstAccess = _DstAccess;
};

// clang-format off
// Common types
namespace Generic
{
using ColorAttachment = RenderPassResource_t<Image, Graphics::MemoryAccess::ColorAttachmentRead>;
using Present = RenderPassResource_t<Image, Graphics::MemoryAccess::Present>;

}  // namespace Res
// clang-format on
}  // namespace lr::Renderer
