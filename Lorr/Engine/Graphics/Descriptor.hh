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
    Count,
};

enum class DescriptorSetLayoutFlag
{
    DescriptorBuffer = 1 << 0, // default flag
    EmbeddedSamplers = 1 << 1,
};
EnumFlags(DescriptorSetLayoutFlag);

struct DescriptorLayoutElement : VkDescriptorSetLayoutBinding
{
    DescriptorLayoutElement(u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize = 1);
};

struct DescriptorSetLayout : APIObject<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
{
    VkDescriptorSetLayout m_pHandle = VK_NULL_HANDLE;
};

struct DescriptorBindingInfo : VkDescriptorBufferBindingInfoEXT
{
    DescriptorBindingInfo(Buffer *pBuffer, BufferUsage bufferUsage);
};

struct DescriptorGetInfo
{
    DescriptorGetInfo(
        u32 binding, DescriptorType type, Buffer *pBuffer, ImageFormat texelFormat = ImageFormat::Unknown);
    DescriptorGetInfo(u32 binding, DescriptorType type, Image *pImage);
    DescriptorGetInfo(u32 binding, DescriptorType type, Sampler *pSampler);

    u32 m_Binding = 0;
    DescriptorType m_Type = DescriptorType::Count;

    union
    {
        VkDescriptorAddressInfoEXT m_BufferInfo = {};
        VkDescriptorImageInfo m_ImageInfo;
        Sampler *m_pSampler;
    };
};

struct WriteDescriptorSet : VkWriteDescriptorSet
{
    WriteDescriptorSet(DescriptorType type, u32 binding, u32 arrayElement, Buffer *pBuffer);
    WriteDescriptorSet(DescriptorType type, u32 binding, u32 arrayElement, Image *pImage, ImageLayout layout);
    WriteDescriptorSet(DescriptorType type, u32 binding, u32 arrayElement, Sampler *pSampler);

    union
    {
        VkDescriptorImageInfo m_ImageInfo = {};
        VkDescriptorBufferInfo m_BufferInfo;
    };
};

}  // namespace lr::Graphics