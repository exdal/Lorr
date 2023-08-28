// Created on Saturday April 22nd 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "APIAllocator.hh"
#include "Common.hh"

#include "Buffer.hh"
#include "Image.hh"

namespace lr::Graphics
{
struct DescriptorLayoutElement : VkDescriptorSetLayoutBinding
{
    DescriptorLayoutElement(u32 binding, DescriptorType type, ShaderStage stage, u32 arraySize = 1);
};

struct DescriptorSetLayout : APIObject<VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>
{
    VkDescriptorSetLayout m_pHandle = VK_NULL_HANDLE;
};

struct DescriptorGetInfo
{
    DescriptorGetInfo() = default;
    DescriptorGetInfo(Buffer *pBuffer, DescriptorType type);
    DescriptorGetInfo(Image *pImage, DescriptorType type);
    DescriptorGetInfo(Sampler *pSampler);

    union
    {
        Buffer *m_pBuffer = nullptr;
        Image *m_pImage;
        Sampler *m_pSampler;
    };

    DescriptorType m_Type = DescriptorType::Sampler;
};

}  // namespace lr::Graphics