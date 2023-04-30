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

}  // namespace lr::Graphics