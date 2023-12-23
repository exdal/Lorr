#pragma once

#include "Graphics/Resource.hh"
#include "TaskTypes.hh"

namespace lr::Graphics
{
/// IMAGES ///

using ImageID = u32;
struct PersistentImageInfo
{
    Image *m_image = nullptr;
    ImageView *m_view = nullptr;
    TaskAccess::Access m_access = TaskAccess::None;
};

struct TaskImageInfo
{
    Image *m_image = nullptr;
    ImageView *m_view = nullptr;
    ImageLayout m_last_layout = ImageLayout::Undefined;
    TaskAccess::Access m_last_access = TaskAccess::None;
    u32 m_last_batch_index = 0;
};

/// BUFFERS ///

using BufferID = u32;
struct PersistentBufferInfo
{
    Buffer *m_buffer = nullptr;
    TaskAccess::Access m_initial_access = TaskAccess::None;
};

struct TaskBufferInfo
{
    Buffer *m_buffer = nullptr;
    TaskAccess::Access m_last_access = TaskAccess::None;
    u32 m_last_batch_index = 0;
};

enum class TaskResourceType
{
    Image,
    Buffer,
};

struct GenericResource
{
    TaskResourceType m_type = TaskResourceType::Image;
    ImageLayout m_image_layout = ImageLayout::Undefined;
    TaskAccess::Access m_access = TaskAccess::None;

    union
    {
        ImageID m_image_id = LR_NULL_ID;
        BufferID m_buffer_id;
    };
};

template<TaskAccess::Access TAccess = TaskAccess::None, ImageLayout TLayout = ImageLayout::Undefined>
struct TaskImageUse : GenericResource
{
    constexpr TaskImageUse()
    {
        m_type = TaskResourceType::Image;
        m_image_layout = TLayout;
        m_access = TAccess;
    }

    constexpr TaskImageUse(ImageID image)
        : TaskImageUse()
    {
        m_image_id = image;
    }
};

template<TaskAccess::Access TAccess>
struct TaskBufferUse : GenericResource
{
    constexpr TaskBufferUse()
    {
        m_type = TaskResourceType::Buffer;
        m_access = TAccess;
    }

    constexpr TaskBufferUse(BufferID buffer)
        : TaskBufferUse()
    {
        m_buffer_id = buffer;
    }
};

namespace Preset
{
    // Common image types
    using ColorAttachment = TaskImageUse<TaskAccess::ColorAttachmentReadWrite, ImageLayout::ColorAttachment>;
    using ClearColorAttachment = TaskImageUse<TaskAccess::ColorAttachmentWrite, ImageLayout::ColorAttachment>;

    // Common buffer types
    using VertexBuffer = TaskBufferUse<TaskAccess::VertexShaderRead>;
}  // namespace Preset
}  // namespace lr::Graphics
