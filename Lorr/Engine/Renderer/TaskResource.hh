#pragma once

#include "Graphics/Resource.hh"
#include "TaskTypes.hh"

namespace lr::Renderer
{
using ImageID = u32;
constexpr static ImageID ImageNull = ~0;

struct PersistentImageInfo
{
    Graphics::Image *m_pImage = nullptr;
    Graphics::ImageView *m_pImageView = nullptr;
    TaskAccess::Access m_InitialAccess = TaskAccess::None;
};

struct TaskImageInfo
{
    Graphics::Image *m_pImage = nullptr;
    Graphics::ImageView *m_pImageView = nullptr;
    Graphics::ImageLayout m_LastLayout = Graphics::ImageLayout::Undefined;
    TaskAccess::Access m_LastAccess = TaskAccess::None;
    u32 m_LastBatchIndex = 0;
};

struct GenericResource
{
    ResourceType m_Type = ResourceType::Buffer;
    ResourceFlag m_Flags = ResourceFlag::None;
    Graphics::ImageLayout m_ImageLayout = Graphics::ImageLayout::Undefined;
    TaskAccess::Access m_Access = TaskAccess::None;
    union
    {
        ImageID m_ImageID = ImageNull;
    };
};

template<
    TaskAccess::Access _Access = TaskAccess::None,
    Graphics::ImageLayout _Layout = Graphics::ImageLayout::Undefined,
    ResourceFlag _Flags = ResourceFlag::None>
struct TaskImageUse : GenericResource
{
    constexpr TaskImageUse()
    {
        m_Type = ToResourceType<Graphics::Image>::kType;
        m_Flags = _Flags;
        m_ImageLayout = _Layout;
        m_Access = _Access;
    }

    constexpr TaskImageUse(ImageID image)
        : TaskImageUse()
    {
        m_ImageID = image;
    }
};

// clang-format off
namespace Preset
{
using namespace lr::Graphics;

// Common types
using SwapChainImage = TaskImageUse<TaskAccess::ColorAttachmentReadWrite, ImageLayout::ColorAttachment, ResourceFlag::SwapChainImage>;
using ColorAttachment = TaskImageUse<TaskAccess::ColorAttachmentReadWrite, ImageLayout::ColorAttachment>;
using Present = TaskImageUse<TaskAccess::BottomOfPipe, ImageLayout::Present>;

}  // namespace Preset
// clang-format on
}  // namespace lr::Renderer
