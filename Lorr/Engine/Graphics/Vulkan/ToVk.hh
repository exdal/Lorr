#pragma once

#include "Engine/Graphics/Vulkan/Impl.hh"

namespace lr {
constexpr static VkBufferUsageFlags to_vk_buffer_flags(vk::BufferUsage v) {
    VkBufferUsageFlags r = 0;
    if (v & vk::BufferUsage::Vertex)
        r |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (v & vk::BufferUsage::Index)
        r |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (v & vk::BufferUsage::TransferSrc)
        r |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (v & vk::BufferUsage::TransferDst)
        r |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (v & vk::BufferUsage::Storage)
        r |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    return r;
}

constexpr static VmaAllocationCreateFlags to_vma_memory_flags(vk::MemoryAllocationFlag v) {
    VmaAllocationCreateFlags r = 0;

    if (v & vk::MemoryAllocationFlag::Dedicated)
        r |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    if (v & vk::MemoryAllocationFlag::HostSeqWrite)
        r |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (v & vk::MemoryAllocationFlag::HostReadWrite)
        r |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    return r;
}

constexpr static VmaMemoryUsage to_vma_usage(vk::MemoryAllocationUsage v) {
    switch (v) {
        case vk::MemoryAllocationUsage::Auto:
            return VMA_MEMORY_USAGE_AUTO;
        case vk::MemoryAllocationUsage::PreferHost:
            return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        case vk::MemoryAllocationUsage::PreferDevice:
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    }
}

constexpr static auto to_vk_image_usage(vk::ImageUsage v) {
    VkImageUsageFlags r = 0;
    if (v & vk::ImageUsage::Sampled)
        r |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (v & vk::ImageUsage::ColorAttachment)
        r |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (v & vk::ImageUsage::DepthStencilAttachment)
        r |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (v & vk::ImageUsage::TransferSrc)
        r |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (v & vk::ImageUsage::TransferDst)
        r |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (v & vk::ImageUsage::Storage)
        r |= VK_IMAGE_USAGE_STORAGE_BIT;

    return r;
}

constexpr static auto to_vk_image_type(vk::ImageType v) {
    switch (v) {
        case vk::ImageType::View1D:
            return VK_IMAGE_TYPE_1D;
        case vk::ImageType::View2D:
            return VK_IMAGE_TYPE_2D;
        case vk::ImageType::View3D:
            return VK_IMAGE_TYPE_3D;
    }
}

constexpr static auto to_vk_image_view_type(vk::ImageViewType v) {
    switch (v) {
        case vk::ImageViewType::View1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case vk::ImageViewType::View2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case vk::ImageViewType::View3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case vk::ImageViewType::Cube:
            return VK_IMAGE_VIEW_TYPE_CUBE;
        case vk::ImageViewType::CubeArray:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case vk::ImageViewType::Array1D:
            return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case vk::ImageViewType::Array2D:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
}

constexpr static auto to_vk_image_aspect_flags(vk::ImageAspectFlag v) {
    VkImageAspectFlags r = 0;

    if (v & vk::ImageAspectFlag::Color)
        r |= VK_IMAGE_ASPECT_COLOR_BIT;
    if (v & vk::ImageAspectFlag::Depth)
        r |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (v & vk::ImageAspectFlag::Stencil)
        r |= VK_IMAGE_ASPECT_STENCIL_BIT;

    return r;
}

constexpr static auto to_vk_filtering(vk::Filtering v) {
    switch (v) {
        case vk::Filtering::Nearest:
            return VK_FILTER_NEAREST;
        case vk::Filtering::Linear:
            return VK_FILTER_LINEAR;
    }
}

constexpr static auto to_vk_address_mode(vk::SamplerAddressMode v) {
    switch (v) {
        case vk::SamplerAddressMode::Repeat:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case vk::SamplerAddressMode::MirroredRepeat:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case vk::SamplerAddressMode::ClampToEdge:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case vk::SamplerAddressMode::ClampToBorder:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
}

constexpr static auto to_vk_mipmap_address_mode(vk::Filtering v) {
    switch (v) {
        case vk::Filtering::Nearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case vk::Filtering::Linear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

constexpr static auto to_vk_compare_op(vk::CompareOp v) {
    switch (v) {
        case vk::CompareOp::Never:
            return VK_COMPARE_OP_NEVER;
        case vk::CompareOp::Less:
            return VK_COMPARE_OP_LESS;
        case vk::CompareOp::Equal:
            return VK_COMPARE_OP_EQUAL;
        case vk::CompareOp::LessEqual:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case vk::CompareOp::Greater:
            return VK_COMPARE_OP_GREATER;
        case vk::CompareOp::NotEqual:
            return VK_COMPARE_OP_NOT_EQUAL;
        case vk::CompareOp::GreaterEqual:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case vk::CompareOp::Always:
            return VK_COMPARE_OP_ALWAYS;
    }
}

constexpr static auto to_vk_shader_stage(vk::ShaderStageFlag v) {
    VkShaderStageFlags r = {};
    if (v & vk::ShaderStageFlag::Vertex)
        r |= VK_SHADER_STAGE_VERTEX_BIT;
    if (v & vk::ShaderStageFlag::Fragment)
        r |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (v & vk::ShaderStageFlag::Compute)
        r |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (v & vk::ShaderStageFlag::TessellationControl)
        r |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (v & vk::ShaderStageFlag::TessellationEvaluation)
        r |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    return static_cast<VkShaderStageFlagBits>(r);
}

constexpr static auto to_vk_primitive_type(vk::PrimitiveType v) {
    switch (v) {
        case vk::PrimitiveType::PointList:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case vk::PrimitiveType::LineList:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case vk::PrimitiveType::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case vk::PrimitiveType::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case vk::PrimitiveType::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case vk::PrimitiveType::Patch:
            return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
}

constexpr static auto to_vk_cull_mode(vk::CullMode v) {
    switch (v) {
        case vk::CullMode::None:
            return VK_CULL_MODE_NONE;
        case vk::CullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case vk::CullMode::Back:
            return VK_CULL_MODE_BACK_BIT;
        case vk::CullMode::FrontBack:
            return VK_CULL_MODE_FRONT_AND_BACK;
    }
}

constexpr static auto to_vk_stencil_op(vk::StencilOp v) {
    switch (v) {
        case vk::StencilOp::Keep:
            return VK_STENCIL_OP_KEEP;
        case vk::StencilOp::Zero:
            return VK_STENCIL_OP_ZERO;
        case vk::StencilOp::Replace:
            return VK_STENCIL_OP_REPLACE;
        case vk::StencilOp::IncrAndClamp:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case vk::StencilOp::DecrAndClamp:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case vk::StencilOp::Invert:
            return VK_STENCIL_OP_INVERT;
        case vk::StencilOp::IncrAndWrap:
            return VK_STENCIL_OP_INCREMENT_AND_WRAP;
        case vk::StencilOp::DecrAndWrap:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
}

constexpr static auto to_vk_blend_factor(vk::BlendFactor v) {
    switch (v) {
        case vk::BlendFactor::Zero:
            return VK_BLEND_FACTOR_ZERO;
        case vk::BlendFactor::One:
            return VK_BLEND_FACTOR_ONE;
        case vk::BlendFactor::SrcColor:
            return VK_BLEND_FACTOR_SRC_COLOR;
        case vk::BlendFactor::InvSrcColor:
            return VK_BLEND_FACTOR_SRC1_COLOR;
        case vk::BlendFactor::SrcAlpha:
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case vk::BlendFactor::InvSrcAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case vk::BlendFactor::DstAlpha:
            return VK_BLEND_FACTOR_DST_ALPHA;
        case vk::BlendFactor::InvDstAlpha:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case vk::BlendFactor::DstColor:
            return VK_BLEND_FACTOR_DST_COLOR;
        case vk::BlendFactor::InvDstColor:
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case vk::BlendFactor::SrcAlphaSat:
            return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
        case vk::BlendFactor::ConstantColor:
            return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case vk::BlendFactor::InvConstantColor:
            return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        case vk::BlendFactor::Src1Color:
            return VK_BLEND_FACTOR_SRC1_COLOR;
        case vk::BlendFactor::InvSrc1Color:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
        case vk::BlendFactor::Src1Alpha:
            return VK_BLEND_FACTOR_SRC1_ALPHA;
        case vk::BlendFactor::InvSrc1Alpha:
            return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    }
}

constexpr static auto to_vk_blend_op(vk::BlendOp v) {
    switch (v) {
        case vk::BlendOp::Add:
            return VK_BLEND_OP_ADD;
        case vk::BlendOp::Subtract:
            return VK_BLEND_OP_SUBTRACT;
        case vk::BlendOp::ReverseSubtract:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case vk::BlendOp::Min:
            return VK_BLEND_OP_MIN;
        case vk::BlendOp::Max:
            return VK_BLEND_OP_MAX;
    }
}

}  // namespace lr
