//
// Created on Tuesday 10th May 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/Base/BaseResource.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKImage : BaseImage
    {
        VkImage m_pHandle = nullptr;
        VkImageView m_pViewHandle = nullptr;

        VkDeviceMemory m_pMemoryHandle = nullptr;

        VkImageLayout m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct VKBuffer : BaseBuffer
    {
        VkBuffer m_pHandle = nullptr;
        VkBufferView m_pViewHandle = nullptr;

        VkDeviceMemory m_pMemoryHandle = nullptr;
    };

    struct VKDescriptorSet : BaseDescriptorSet
    {
        VkDescriptorSet pHandle = nullptr;
        VkDescriptorSetLayout pSetLayoutHandle = nullptr;
    };

}  // namespace lr::Graphics
