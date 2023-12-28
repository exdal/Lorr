#pragma once

#include "TaskTypes.hh"

namespace lr::Graphics
{
/// IMAGES ///

LR_HANDLE(ImageID, u32);
struct PersistentImageInfo
{
    ImageID m_image_id = {};
    ImageID m_image_view_id = {};
    TaskAccess::Access m_access = TaskAccess::None;
};

struct TaskImageInfo
{
    ImageID m_image_id = {};
    ImageID m_image_view_id = {};

    ImageLayout m_last_layout = ImageLayout::Undefined;
    TaskAccess::Access m_last_access = TaskAccess::None;
    u32 m_last_batch_index = 0;
};

/// BUFFERS ///

LR_HANDLE(BufferID, u32);
struct PersistentBufferInfo
{
    BufferID m_buffer_id = {};
    TaskAccess::Access m_initial_access = TaskAccess::None;
};

struct TaskBufferInfo
{
    BufferID m_buffer_id = {};
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
        ImageID m_task_image_id = {};
        BufferID m_task_buffer_id;
    };
};

template<TaskAccess::Access TAccess = TaskAccess::None, ImageLayout LayoutT = ImageLayout::Undefined>
struct TaskImageUse : GenericResource
{
    constexpr TaskImageUse()
    {
        m_type = TaskResourceType::Image;
        m_image_layout = LayoutT;
        m_access = TAccess;
    }

    constexpr TaskImageUse(const ImageID& image)
        : TaskImageUse()
    {
        m_task_image_id = image;
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

    constexpr TaskBufferUse(const BufferID& buffer)
        : TaskBufferUse()
    {
        m_task_buffer_id = buffer;
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
