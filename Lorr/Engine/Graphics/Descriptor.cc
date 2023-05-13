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
DescriptorGetInfo::DescriptorGetInfo(Buffer *pBuffer, ImageFormat texelFormat)
    : m_DescriptorIndex(pBuffer->m_DescriptorIndex)
{
    m_BufferInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
        .pNext = nullptr,
        .address = pBuffer->m_DeviceAddress,
        .range = pBuffer->m_DeviceDataLen,
        .format = VK::ToFormat(texelFormat),
    };
}

DescriptorGetInfo::DescriptorGetInfo(Image *pImage)
    : m_DescriptorIndex(pImage->m_DescriptorIndex)
{
    m_ImageInfo = {
        .imageView = pImage->m_pViewHandle,
        // .imageLayout  /// is ignored if `descriptorBufferImageLayoutIgnored` is enabled
    };
}

DescriptorGetInfo::DescriptorGetInfo(Sampler *pSampler)
    : m_DescriptorIndex(pSampler->m_DescriptorIndex)
{
}

}  // namespace lr::Graphics