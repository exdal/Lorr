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

DescriptorBindingPushDescriptor::DescriptorBindingPushDescriptor(Buffer *pBuffer)
{
    this->sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT;
    this->pNext = nullptr;
    this->buffer = pBuffer->m_pHandle;
}

DescriptorBindingInfo::DescriptorBindingInfo(
    DescriptorBindingPushDescriptor *pPushDescriptorInfo, Buffer *pBuffer, BufferUsage bufferUsage)
{
    this->sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    this->pNext = pPushDescriptorInfo;
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

}  // namespace lr::Graphics