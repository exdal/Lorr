#include "Resource.hh"

namespace lr::Graphics
{

ImageSubresourceRange::ImageSubresourceRange(ImageSubresourceInfo info)
{
    this->aspectMask = static_cast<VkImageAspectFlags>(info.m_AspectMask);
    this->baseMipLevel = info.m_BaseMip;
    this->levelCount = info.m_MipCount;
    this->baseArrayLayer = info.m_BaseSlice;
    this->layerCount = info.m_SliceCount;
}

DescriptorLayoutElement::DescriptorLayoutElement(
    u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize)
{
    this->binding = binding;
    this->descriptorType = (VkDescriptorType)type;
    this->stageFlags = (VkShaderStageFlags)stage;
    this->descriptorCount = arraySize;
    this->pImmutableSamplers = nullptr;  // No need for static samplers, bindless good
}

/// `VkDescriptorDataEXT` is an union so var names such as `data.pUniformTexelBuffer`
/// do not matter, only thing that matters is type
DescriptorGetInfo::DescriptorGetInfo(Buffer *pBuffer, u64 dataSize, DescriptorType type)
    : m_BufferAddress(pBuffer->m_DeviceAddress),
      m_DataSize(dataSize),
      m_Type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(ImageView *pImageView, DescriptorType type)
    : m_pImageView(pImageView),
      m_Type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(Sampler *pSampler)
    : m_pSampler(pSampler),
      m_Type(DescriptorType::Sampler)
{
}

}  // namespace lr::Graphics