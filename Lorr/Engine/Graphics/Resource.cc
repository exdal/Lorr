#include "Resource.hh"

namespace lr::Graphics
{
ImageSubresourceRange::ImageSubresourceRange(ImageSubresourceInfo info)
{
    this->aspectMask = static_cast<VkImageAspectFlags>(info.m_aspect_mask);
    this->baseMipLevel = info.m_base_mip;
    this->levelCount = info.m_mip_count;
    this->baseArrayLayer = info.m_base_slice;
    this->layerCount = info.m_slice_count;
}

DescriptorLayoutElement::DescriptorLayoutElement(u32 binding, DescriptorType type, ShaderStage stage, u32 array_size)
    : VkDescriptorSetLayoutBinding()
{
    this->binding = binding;
    this->descriptorType = static_cast<VkDescriptorType>(type);
    this->stageFlags = static_cast<VkShaderStageFlags>(stage);
    this->descriptorCount = array_size;
    this->pImmutableSamplers = nullptr;  // No need for static samplers, bindless good
}

/// `VkDescriptorDataEXT` is an union so var names such as `data.pUniformTexelBuffer`
/// do not matter, only thing that matters is type
DescriptorGetInfo::DescriptorGetInfo(Buffer *buffer, u64 data_size, DescriptorType type)
    : m_buffer_address(buffer->m_device_address),
      m_data_size(data_size),
      m_type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(ImageView *image_view, DescriptorType type)
    : m_image_view(image_view),
      m_type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(Sampler *sampler)
    : m_sampler(sampler),
      m_type(DescriptorType::Sampler)
{
}

}  // namespace lr::Graphics