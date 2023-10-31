#include "Resource.hh"

namespace lr::Graphics
{
VkImageSubresourceRange ImageView::GetSubresourceRange()
{
    return (VkImageSubresourceRange){
        .aspectMask = static_cast<VkImageAspectFlags>(m_Aspect),
        .baseMipLevel = m_BaseMip,
        .levelCount = m_MipCount,
        .baseArrayLayer = m_BaseLayer,
        .layerCount = m_LayerCount,
    };
}

VkImageSubresourceLayers ImageView::GetSubresourceLayers()
{
    return (VkImageSubresourceLayers){
        .aspectMask = static_cast<VkImageAspectFlags>(m_Aspect),
        .mipLevel = m_BaseMip,
        .baseArrayLayer = m_BaseLayer,
        .layerCount = m_LayerCount,
    };
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