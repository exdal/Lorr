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

DescriptorLayoutElement::DescriptorLayoutElement(
    u32 binding, DescriptorType type, ShaderStage stage, u32 max_descriptors, DescriptorBindingFlag flags)
{
    m_binding_info = {};
    m_binding_info.binding = binding;
    m_binding_info.descriptorType = static_cast<VkDescriptorType>(type);
    m_binding_info.stageFlags = static_cast<VkShaderStageFlags>(stage);
    m_binding_info.descriptorCount = max_descriptors;
    m_binding_info.pImmutableSamplers = nullptr;  // No need for static samplers, bindless good
    m_binding_flag = flags;
}

ImageDescriptorInfo::ImageDescriptorInfo(ImageView *image_view, ImageLayout layout)
    : VkDescriptorImageInfo(VK_NULL_HANDLE, image_view->m_handle, static_cast<VkImageLayout>(layout))
{
}

BufferDescriptorInfo::BufferDescriptorInfo(Buffer *buffer, u64 size, u64 offset)
    : VkDescriptorBufferInfo(buffer->m_handle, offset, size)
{
}

SamplerDescriptorInfo::SamplerDescriptorInfo(Sampler *sampler)
    : VkDescriptorImageInfo(sampler->m_handle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED)
{
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

DescriptorPoolSize::DescriptorPoolSize(DescriptorType type, u32 count)
    : VkDescriptorPoolSize()
{
    this->type = static_cast<VkDescriptorType>(type);
    this->descriptorCount = count;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorSet *descriptor_set, ImageDescriptorInfo &image_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : WriteDescriptorSet(descriptor_set, dst_type, dst_binding, dst_element, dst_count)
{
    this->pImageInfo = &image_info;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorSet *descriptor_set, BufferDescriptorInfo &buffer_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : WriteDescriptorSet(descriptor_set, dst_type, dst_binding, dst_element, dst_count)
{
    this->pBufferInfo = &buffer_info;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorSet *descriptor_set, SamplerDescriptorInfo &sampler_info, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : WriteDescriptorSet(descriptor_set, dst_type, dst_binding, dst_element, dst_count)
{
    this->pImageInfo = &sampler_info;
}

WriteDescriptorSet::WriteDescriptorSet(DescriptorSet *descriptor_set, DescriptorType dst_type, u32 dst_binding, u32 dst_element, u32 dst_count)
    : VkWriteDescriptorSet(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr)
{
    this->dstSet = descriptor_set->m_handle;
    this->dstBinding = dst_binding;
    this->dstArrayElement = dst_element;
    this->descriptorCount = dst_count;
    this->descriptorType = static_cast<VkDescriptorType>(dst_type);
}

CopyDescriptorSet::CopyDescriptorSet(
    DescriptorSet *src, u32 src_binding, u32 src_element, DescriptorSet *dst, u32 dst_binding, u32 dst_element, u32 count)
    : VkCopyDescriptorSet(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET, nullptr)
{
    this->srcSet = *src;
    this->srcBinding = src_binding;
    this->srcArrayElement = src_element;

    this->dstSet = *dst;
    this->dstBinding = dst_binding;
    this->dstArrayElement = dst_element;

    this->descriptorCount = count;
}

}  // namespace lr::Graphics