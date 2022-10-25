//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseResource.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKImage : public BaseImage
    {
        VkImage m_pHandle = nullptr;
        VkImageView m_pViewHandle = nullptr;
        VkSampler m_pSampler = nullptr;

        VkDeviceMemory m_pMemoryHandle = nullptr;

        u32 m_RequiredDataSize = 0;  // Required data size from Vulkan API

        VkImageLayout m_FinalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VKBuffer : public BaseBuffer
    {
        VkBuffer m_pHandle = nullptr;
        VkBufferView m_pViewHandle = nullptr;
        VkDeviceMemory m_pMemoryHandle = nullptr;
    };

    /// -------------- Descriptors --------------- ///

    struct VKDescriptorBindingDesc
    {
        // u32 BindingID = -1;
        DescriptorType Type;
        VkShaderStageFlagBits ShaderStageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        u32 ArraySize = 1;

        VKBuffer *pBuffer = nullptr;
        VKImage *pImage = nullptr;
    };

    struct VKDescriptorSetDesc
    {
        u32 BindingCount = 0;
        VKDescriptorBindingDesc pBindings[8];
    };

    struct VKDescriptorSet
    {
        u32 BindingCount = 0;
        VKDescriptorBindingDesc pBindingInfos[8];
        VkDescriptorBufferInfo pBindingBufferInfos[8];

        VkDescriptorSet pHandle = nullptr;
        VkDescriptorSetLayout pSetLayoutHandle = nullptr;
    };

    /// ------------------------------------------

}  // namespace lr::Graphics
