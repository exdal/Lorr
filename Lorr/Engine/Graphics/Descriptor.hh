//
// Created on Saturday 22nd April 2023 by exdal
//

#pragma once

#include "APIAllocator.hh"

#include "Buffer.hh"
#include "Image.hh"
#include "Shader.hh"

namespace lr::Graphics
{
enum class DescriptorType : u32
{
    Sampler = 0,
    SampledImage,   // Read only image
    UniformBuffer,  // Read only buffer
    StorageImage,   // RW image
    StorageBuffer,  // RW Buffer
    StaticSampler,  // EXT_descriptor_buffer
    Count,
};

enum class DescriptorSetLayoutType
{
    None = 0,
    PushDescriptor = 1,
    EmbeddedSamplers = 2,
};

struct DescriptorLayoutElement : VkDescriptorSetLayoutBinding
{
    DescriptorLayoutElement(u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize = 1);
};

struct DescriptorSetLayout : APIObject<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
{
    DescriptorSetLayoutType m_Type = DescriptorSetLayoutType::None;
    VkDescriptorSetLayout m_pHandle = VK_NULL_HANDLE;
};

struct DescriptorBindingPushDescriptor : VkDescriptorBufferBindingPushDescriptorBufferHandleEXT
{
    DescriptorBindingPushDescriptor(Buffer *pBuffer);
};

struct DescriptorBindingInfo : VkDescriptorBufferBindingInfoEXT
{
    DescriptorBindingInfo(
        DescriptorBindingPushDescriptor *pPushDescriptorInfo, Buffer *pBuffer, BufferUsage bufferUsage);
};
}  // namespace lr::Graphics