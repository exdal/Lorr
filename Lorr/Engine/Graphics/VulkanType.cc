// Created on Saturday April 22nd 2023 by exdal
// Last modified on Sunday May 28th 2023 by exdal

#include "VulkanType.hh"

namespace lr::Graphics
{
constexpr VkFormat kFormatLUT[] = {
    VK_FORMAT_UNDEFINED,            // Unknown
    VK_FORMAT_R8G8B8A8_UNORM,       // RGBA8F
    VK_FORMAT_R8G8B8A8_SRGB,        // RGBA8_SRGBF
    VK_FORMAT_B8G8R8A8_UNORM,       // BGRA8F
    VK_FORMAT_R16G16B16A16_SFLOAT,  // RGBA16F
    VK_FORMAT_R32G32B32A32_SFLOAT,  // RGBA32F
    VK_FORMAT_R32_UINT,             // R32U
    VK_FORMAT_R32_SFLOAT,           // R32F
    VK_FORMAT_D32_SFLOAT,           // D32F
    VK_FORMAT_D32_SFLOAT_S8_UINT,   // D32FS8U
};

constexpr VkPrimitiveTopology kPrimitiveLUT[] = {
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,      // PointList
    VK_PRIMITIVE_TOPOLOGY_LINE_LIST,       // LineList
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,      // LineStrip
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,   // TriangleList
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,  // TriangleStrip
    VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // Patch
};

constexpr VkCullModeFlags kCullModeLUT[] = {
    VK_CULL_MODE_NONE,       // None
    VK_CULL_MODE_FRONT_BIT,  // Front
    VK_CULL_MODE_BACK_BIT,   // Back
};

constexpr VkDescriptorType kDescriptorTypeLUT[] = {
    VK_DESCRIPTOR_TYPE_SAMPLER,         // LR_DESCRIPTOR_TYPE_SAMPLER
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,   // LR_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  // LR_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   // LR_DESCRIPTOR_TYPE_STORAGE_IMAGE
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  // LR_DESCRIPTOR_TYPE_STORAGE_BUFFER
    VK_DESCRIPTOR_TYPE_SAMPLER,         // LR_DESCRIPTOR_TYPE_SAMPLER
};

constexpr VkImageLayout kImageLayoutLUT[] = {
    VK_IMAGE_LAYOUT_UNDEFINED,                         // LR_IMAGE_LAYOUT_UNDEFINED
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,                   // LR_IMAGE_LAYOUT_PRESENT
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,          // LR_IMAGE_LAYOUT_COLOR_ATTACHMENT
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  // LR_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,          // LR_IMAGE_LAYOUT_COLOR_READ_ONLY
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,   // LR_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,              // LR_IMAGE_LAYOUT_TRANSFER_SRC
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,              // LR_IMAGE_LAYOUT_TRANSFER_DST
    VK_IMAGE_LAYOUT_GENERAL,                           // LR_IMAGE_LAYOUT_GENERAL
};

VkFormat VK::ToFormat(ImageFormat format)
{
    return kFormatLUT[(u32)format];
}

VkPrimitiveTopology VK::ToTopology(PrimitiveType type)
{
    return kPrimitiveLUT[(u32)type];
}

VkCullModeFlags VK::ToCullMode(CullMode mode)
{
    return kCullModeLUT[(u32)mode];
}

VkDescriptorType VK::ToDescriptorType(DescriptorType type)
{
    return kDescriptorTypeLUT[(u32)type];
}

VkImageUsageFlags VK::ToImageUsage(ImageUsage usage)
{
    u32 v = 0;

    if (usage & ImageUsage::ColorAttachment)
        v |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (usage & ImageUsage::DepthStencilAttachment)
        v |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (usage & ImageUsage::Sampled)
        v |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (usage & ImageUsage::TransferSrc)
        v |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (usage & ImageUsage::TransferDst)
        v |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (usage & ImageUsage::Storage)
        v |= VK_IMAGE_USAGE_STORAGE_BIT;

    return (VkImageUsageFlags)v;
}

VkBufferUsageFlags VK::ToBufferUsage(BufferUsage usage)
{
    u32 v = 0;

    if (usage & BufferUsage::Vertex)
        v |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (usage & BufferUsage::Index)
        v |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (usage & BufferUsage::Uniform)
        v |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (usage & BufferUsage::TransferSrc)
        v |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (usage & BufferUsage::TransferDst)
        v |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (usage & BufferUsage::Storage)
        v |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (usage & BufferUsage::SamplerDescriptor)
        v |= VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

    if (usage & BufferUsage::ResourceDescriptor)
        v |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    return (VkBufferUsageFlags)v;
}

VkImageLayout VK::ToImageLayout(ImageLayout layout)
{
    return kImageLayoutLUT[(u32)layout];
}

VkShaderStageFlagBits VK::ToShaderType(ShaderStage type)
{
    u64 v = 0;

    if (type & ShaderStage::Vertex)
        v |= VK_SHADER_STAGE_VERTEX_BIT;

    if (type & ShaderStage::Pixel)
        v |= VK_SHADER_STAGE_FRAGMENT_BIT;

    if (type & ShaderStage::Compute)
        v |= VK_SHADER_STAGE_COMPUTE_BIT;

    if (type & ShaderStage::TessellationControl)
        v |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

    if (type & ShaderStage::TessellationEvaluation)
        v |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    return (VkShaderStageFlagBits)v;
}

VkPipelineStageFlags2 VK::ToPipelineStage(PipelineStage stage)
{
    u64 v = 0;

    if (stage == PipelineStage::None)
        return VK_PIPELINE_STAGE_2_NONE;

    if (stage & PipelineStage::DrawIndirect)
        v |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;

    if (stage & PipelineStage::VertexAttribInput)
        v |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;

    if (stage & PipelineStage::IndexAttribInput)
        v |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;

    if (stage & PipelineStage::VertexShader)
        v |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;

    if (stage & PipelineStage::TessellationControl)
        v |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;

    if (stage & PipelineStage::TessellationEvaluation)
        v |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;

    if (stage & PipelineStage::PixelShader)
        v |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    if (stage & PipelineStage::EarlyPixelTests)
        v |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;

    if (stage & PipelineStage::LatePixelTests)
        v |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

    if (stage & PipelineStage::ColorAttachmentOutput)
        v |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    if (stage & PipelineStage::AllGraphics)
        v |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    if (stage & PipelineStage::ComputeShader)
        v |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;

    if (stage & PipelineStage::Host)
        v |= VK_PIPELINE_STAGE_2_HOST_BIT;

    if (stage & PipelineStage::Copy)
        v |= VK_PIPELINE_STAGE_2_COPY_BIT;

    if (stage & PipelineStage::Bilt)
        v |= VK_PIPELINE_STAGE_2_BLIT_BIT;

    if (stage & PipelineStage::Resolve)
        v |= VK_PIPELINE_STAGE_2_RESOLVE_BIT;

    if (stage & PipelineStage::Clear)
        v |= VK_PIPELINE_STAGE_2_CLEAR_BIT;

    if (stage & PipelineStage::AllTransfer)
        v |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;

    if (stage & PipelineStage::AllCommands)
        v |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    if (stage & PipelineStage::BottomOfPipe)
        v |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;

    return v;
}

VkAccessFlags2 VK::ToAccessFlags(MemoryAccess access)
{
    u64 v = 0;

    if (access == MemoryAccess::None)
        return VK_ACCESS_2_NONE;

    if (access & MemoryAccess::VertexAttribRead)
        v |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;

    if (access & MemoryAccess::IndexAttribRead)
        v |= VK_ACCESS_2_INDEX_READ_BIT;

    if (access & MemoryAccess::InputAttachmentRead)
        v |= VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;

    if (access & MemoryAccess::UniformRead)
        v |= VK_ACCESS_2_UNIFORM_READ_BIT;

    if (access & MemoryAccess::SampledRead)
        v |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

    if (access & MemoryAccess::StorageRead)
        v |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

    if (access & MemoryAccess::StorageWrite)
        v |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

    if (access & MemoryAccess::ColorAttachmentRead)
        v |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

    if (access & MemoryAccess::ColorAttachmentWrite)
        v |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    if (access & MemoryAccess::DepthStencilRead)
        v |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    if (access & MemoryAccess::DepthStencilWrite)
        v |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (access & MemoryAccess::TransferRead)
        v |= VK_ACCESS_2_TRANSFER_READ_BIT;

    if (access & MemoryAccess::TransferWrite)
        v |= VK_ACCESS_2_TRANSFER_WRITE_BIT;

    if (access & MemoryAccess::HostRead)
        v |= VK_ACCESS_2_HOST_READ_BIT;

    if (access & MemoryAccess::HostWrite)
        v |= VK_ACCESS_2_HOST_WRITE_BIT;

    if (access & MemoryAccess::MemoryRead)
        v |= VK_ACCESS_2_MEMORY_READ_BIT;

    if (access & MemoryAccess::MemoryWrite)
        v |= VK_ACCESS_2_MEMORY_WRITE_BIT;

    if (access & MemoryAccess::Present)
        v |= VK_ACCESS_2_NONE;

    return v;
}
}  // namespace lr::Graphics