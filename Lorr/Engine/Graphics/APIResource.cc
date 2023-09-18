#include "APIResource.hh"

namespace lr::Graphics
{
DescriptorLayoutElement::DescriptorLayoutElement(
    u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize)
{
    this->binding = binding;
    this->descriptorType = (VkDescriptorType)type;
    this->stageFlags = (VkShaderStageFlags)stage;
    this->descriptorCount = arraySize;
    this->pImmutableSamplers = nullptr;  // TODO: static samplers
}

/// `VkDescriptorDataEXT` is an union so var names such as `data.pUniformTexelBuffer`
/// do not matter, only thing that matters is type
DescriptorGetInfo::DescriptorGetInfo(Buffer *pBuffer, DescriptorType type)
    : m_pBuffer(pBuffer),
      m_Type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(Image *pImage, DescriptorType type)
    : m_pImage(pImage),
      m_Type(type)
{
}

DescriptorGetInfo::DescriptorGetInfo(Sampler *pSampler)
    : m_pSampler(pSampler),
      m_Type(DescriptorType::Sampler)
{
}

}  // namespace lr::Graphics