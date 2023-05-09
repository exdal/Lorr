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

DescriptorBindingInfo::DescriptorBindingInfo(Buffer *pBuffer, BufferUsage bufferUsage)
{
    this->sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    this->pNext = nullptr;
    this->address = pBuffer->m_DeviceAddress;
    this->usage = VK::ToBufferUsage(bufferUsage);
}

/// `VkDescriptorDataEXT` is an union so var names such as `data.pUniformTexelBuffer`
/// do not matter, only thing that matters is type
DescriptorGetInfo::DescriptorGetInfo(u32 binding, DescriptorType type, Buffer *pBuffer, ImageFormat texelFormat)
    : m_Binding(binding),
      m_Type(type)
{
    m_BufferInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
        .pNext = nullptr,
        .address = pBuffer->m_DeviceAddress,
        .range = pBuffer->m_DeviceDataLen,
        .format = VK::ToFormat(texelFormat),
    };
}

DescriptorGetInfo::DescriptorGetInfo(u32 binding, DescriptorType type, Image *pImage)
    : m_Binding(binding),
      m_Type(type)
{
    m_ImageInfo = {
        .imageView = pImage->m_pViewHandle,
        // .imageLayout  /// is ignored if `descriptorBufferImageLayoutIgnored` is enabled
    };
}

DescriptorGetInfo::DescriptorGetInfo(u32 binding, DescriptorType type, Sampler *pSampler)
    : m_Binding(binding),
      m_Type(DescriptorType::Sampler),
      m_pSampler(pSampler)
{
}

WriteDescriptorSet::WriteDescriptorSet(DescriptorType type, u32 binding, u32 arrayElement, Buffer *pBuffer)
{
    this->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    this->pNext = nullptr;
    this->descriptorCount = 1;  // TODO
    this->descriptorType = VK::ToDescriptorType(type);
    this->dstBinding = binding;
    this->dstArrayElement = arrayElement;
    this->pBufferInfo = &m_BufferInfo;

    m_BufferInfo.buffer = pBuffer->m_pHandle;
    m_BufferInfo.offset = 0;  // TODO
    m_BufferInfo.range = pBuffer->m_DataLen;
}

WriteDescriptorSet::WriteDescriptorSet(
    DescriptorType type, u32 binding, u32 arrayElement, Image *pImage, ImageLayout layout)
{
    this->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    this->pNext = nullptr;
    this->descriptorCount = 1;  // TODO
    this->descriptorType = VK::ToDescriptorType(type);
    this->dstBinding = binding;
    this->dstArrayElement = arrayElement;
    this->pImageInfo = &m_ImageInfo;

    m_ImageInfo.imageView = pImage->m_pViewHandle;
    m_ImageInfo.imageLayout = VK::ToImageLayout(layout);
    m_ImageInfo.sampler = nullptr;
}

WriteDescriptorSet::WriteDescriptorSet(DescriptorType type, u32 binding, u32 arrayElement, Sampler *pSampler)
{
    this->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    this->pNext = nullptr;
    this->descriptorCount = 1;  // TODO
    this->descriptorType = VK::ToDescriptorType(type);
    this->dstBinding = binding;
    this->dstArrayElement = arrayElement;
    this->pImageInfo = &m_ImageInfo;

    m_ImageInfo.imageView = nullptr;
    m_ImageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_ImageInfo.sampler = pSampler->m_pHandle;
}

}  // namespace lr::Graphics