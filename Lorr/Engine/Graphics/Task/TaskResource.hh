#pragma once

#include "Engine/Graphics/Vulkan.hh"

namespace lr {
enum class TaskImageID : u32 { Invalid = ~0_u32 };
enum class TaskBufferID : u32 { Invalid = ~0_u32 };

struct TaskImageInfo {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    vk::ImageLayout layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl access = vk::PipelineAccess::TopOfPipe;
    usize last_submit_index = 0;
};

struct TaskBufferInfo {
    BufferID buffer_id = BufferID::Invalid;
    vk::PipelineAccessImpl last_access = vk::PipelineAccess::None;
};

enum class TaskImageFlag {
    None = 0,
    SwapChainRelative = 1 << 0,
};
consteval void enable_bitmask(TaskImageFlag);

struct TaskImage {
    ImageID image_id = ImageID::Invalid;
    ImageViewID image_view_id = ImageViewID::Invalid;
    vk::ImageSubresourceRange subresource_range = {};
    TaskImageFlag flags = TaskImageFlag::None;
    vk::ImageLayout last_layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl last_access = vk::PipelineAccess::None;

    usize last_submit_index = 0;
};

enum class TaskUseType : u32 { Buffer = 0, Image };
struct TaskUse {
    TaskUseType type = TaskUseType::Buffer;
    vk::ImageLayout image_layout = vk::ImageLayout::Undefined;
    vk::PipelineAccessImpl access = vk::PipelineAccess::None;
    union {
        TaskBufferID task_buffer_id = TaskBufferID::Invalid;
        TaskImageID task_image_id;
        u32 index;
    };
};

template<vk::PipelineAccessImpl AccessT>
struct TaskBufferUse : TaskUse {
    constexpr TaskBufferUse() {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
    }

    constexpr TaskBufferUse(TaskBufferID id) {
        this->type = TaskUseType::Buffer;
        this->access = AccessT;
        this->task_buffer_id = id;
    };
};

template<vk::ImageLayout LayoutT, vk::PipelineAccessImpl AccessT>
struct TaskImageUse : TaskUse {
    constexpr TaskImageUse() {
        this->type = TaskUseType::Image;
        this->image_layout = LayoutT;
        this->access = AccessT;
    }

    constexpr TaskImageUse(TaskImageID id) {
        this->type = TaskUseType::Image;
        this->image_layout = LayoutT;
        this->access = AccessT;
        this->task_image_id = id;
    };
};

// Task Use Presets
namespace Preset {
    using ColorAttachmentWrite = TaskImageUse<vk::ImageLayout::ColorAttachment, vk::PipelineAccess::ColorAttachmentReadWrite>;
    using ColorAttachmentRead = TaskImageUse<vk::ImageLayout::ColorAttachment, vk::PipelineAccess::ColorAttachmentRead>;
    using DepthAttachmentWrite = TaskImageUse<vk::ImageLayout::DepthAttachment, vk::PipelineAccess::DepthStencilReadWrite>;
    using DepthAttachmentRead = TaskImageUse<vk::ImageLayout::DepthAttachment, vk::PipelineAccess::DepthStencilRead>;
    using ColorReadOnly = TaskImageUse<vk::ImageLayout::ColorReadOnly, vk::PipelineAccess::FragmentShaderRead>;
    using ComputeWrite = TaskImageUse<vk::ImageLayout::General, vk::PipelineAccess::ComputeWrite>;
    using ComputeRead = TaskImageUse<vk::ImageLayout::General, vk::PipelineAccess::ComputeRead>;
    using BlitRead = TaskImageUse<vk::ImageLayout::TransferSrc, vk::PipelineAccess::BlitRead>;
    using BlitWrite = TaskImageUse<vk::ImageLayout::TransferDst, vk::PipelineAccess::BlitWrite>;

    using VertexBuffer = TaskBufferUse<vk::PipelineAccess::VertexAttrib>;
    using IndexBuffer = TaskBufferUse<vk::PipelineAccess::IndexAttrib>;
};  // namespace Preset

}  // namespace lr
