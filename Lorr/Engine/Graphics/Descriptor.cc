// Created on Tuesday April 25th 2023 by exdal
// Last modified on Friday August 25th 2023 by exdal

#include "Descriptor.hh"

#include "VulkanType.hh"

namespace lr::Graphics
{
DescriptorLayoutElement::DescriptorLayoutElement(
    u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize)
{
    this->binding = binding;
    this->descriptorType = VK::ToDescriptorType(type);
    this->stageFlags = VK::ToShaderType(stage);
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