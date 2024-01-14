#pragma once

#include "Graphics/Resource.hh"
#include "TaskTypes.hh"

namespace lr::Graphics
{
/// IMAGES ///

struct PersistentImageInfo
{
    ImageID m_image_id = ImageID::Invalid;
    ImageViewID m_image_view_id = ImageViewID::Invalid;
    TaskAccess::Access m_access = TaskAccess::None;
};

LR_HANDLE(TaskImageID, u32);
struct TaskImageInfo
{
    ImageID m_image_id = ImageID::Invalid;
    ImageViewID m_image_view_id = ImageViewID::Invalid;

    ImageLayout m_last_layout = ImageLayout::Undefined;
    TaskAccess::Access m_last_access = TaskAccess::None;
    u32 m_first_submit_scope_index = 0;
    u32 m_last_batch_index = 0;
    u32 m_last_submit_scope_index = 0;
};

/// BUFFERS ///

struct PersistentBufferInfo
{
    BufferID m_buffer_id = BufferID::Invalid;
    TaskAccess::Access m_initial_access = TaskAccess::None;
};

LR_HANDLE(TaskBufferID, u32);
struct TaskBufferInfo
{
    BufferID m_buffer_id = BufferID::Invalid;
    TaskAccess::Access m_last_access = TaskAccess::None;
    u32 m_last_batch_index = 0;
    u32 m_last_submit_scope_index = 0;
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
        TaskImageID m_task_image_id = TaskImageID::Invalid;
        TaskBufferID m_task_buffer_id;
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

    constexpr TaskImageUse(TaskImageID task_image_id)
        : TaskImageUse()
    {
        m_task_image_id = task_image_id;
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

    constexpr TaskBufferUse(TaskBufferID task_buffer_id)
        : TaskBufferUse()
    {
        m_task_buffer_id = task_buffer_id;
    }
};

namespace Preset
{
    // Common image types
    using ColorAttachment = TaskImageUse<TaskAccess::ColorAttachmentReadWrite, ImageLayout::ColorAttachment>;
    using ClearColorAttachment = TaskImageUse<TaskAccess::ColorAttachmentWrite, ImageLayout::ColorAttachment>;
    using PixelReadOnly = TaskImageUse<TaskAccess::PixelShaderRead, ImageLayout::ColorReadOnly>;

    // Common buffer types
    using VertexBuffer = TaskBufferUse<TaskAccess::VertexShaderRead>;
}  // namespace Preset
}  // namespace lr::Graphics
