#pragma once

#include "Graphics/Resource.hh"

#include <EASTL/vector.h>

namespace lr::Renderer
{
enum class ResourceType : u32
{
    Buffer,
    Image,
};

enum class ResourceFlag : u32
{
    None = 0,
    SwapChainImage = 1 << 0,
    SwapChainRelative = 1 << 1,  // for scaling
};
LR_TYPEOP_ARITHMETIC_INT(ResourceFlag, ResourceFlag, &);

template<typename _T>
struct ToResourceType;

template<>
struct ToResourceType<Graphics::Image>
{
    constexpr static ResourceType kType = ResourceType::Image;
};

template<>
struct ToResourceType<Graphics::Buffer>
{
    constexpr static ResourceType kType = ResourceType::Buffer;
};

struct GenericResource
{
    ResourceType m_Type = ResourceType::Buffer;
    ResourceFlag m_Flags = ResourceFlag::None;
    union
    {
        Graphics::Buffer *m_pBuffer = nullptr;
        Graphics::Image *m_pImage;
    };
    Graphics::ImageLayout m_ImageLayout = Graphics::ImageLayout::Undefined;
    Graphics::MemoryAccess m_Access = Graphics::MemoryAccess::None;
    Graphics::PipelineStage m_Stage = Graphics::PipelineStage::None;
};

template<
    Graphics::MemoryAccess _Access,
    Graphics::PipelineStage _Stage,
    Graphics::ImageLayout _Layout,
    ResourceFlag _Flags = ResourceFlag::None>
struct PresetImage : GenericResource
{
    constexpr PresetImage()
    {
        m_Type = ToResourceType<Graphics::Image>::kType;
        m_Flags = _Flags;
        m_Access = _Access;
        m_Stage = _Stage;
        m_ImageLayout = _Layout;
    }
    constexpr PresetImage(Graphics::Image *pImage)
        : PresetImage()
    {
        m_pImage = pImage;
    }
};

// clang-format off
namespace Preset
{
using namespace lr::Graphics;

// Common types
using SwapChainImage = PresetImage<MemoryAccess::ColorAttachmentAll, PipelineStage::ColorAttachmentOutput, ImageLayout::ColorAttachment, ResourceFlag::SwapChainImage>;
using ColorAttachment = PresetImage<MemoryAccess::ColorAttachmentRead, PipelineStage::ColorAttachmentOutput, ImageLayout::ColorAttachment>;
using Present = PresetImage<MemoryAccess::None, PipelineStage::ColorAttachmentOutput, ImageLayout::ColorAttachment>;

}  // namespace Preset
// clang-format on
}  // namespace lr::Renderer
