// Created on Saturday April 22nd 2023 by exdal
// Last modified on Saturday August 26th 2023 by exdal

#pragma once

#include "Buffer.hh"
#include "CommandList.hh"
#include "Descriptor.hh"
#include "Pipeline.hh"
#include "Image.hh"

namespace lr::Graphics::VK
{
VkFormat ToFormat(ImageFormat format);
VkPrimitiveTopology ToTopology(PrimitiveType type);
VkCullModeFlags ToCullMode(CullMode mode);
VkDescriptorType ToDescriptorType(DescriptorType type);
VkImageUsageFlags ToImageUsage(ImageUsage usage);
VkBufferUsageFlags ToBufferUsage(BufferUsage usage);
VkImageLayout ToImageLayout(ImageLayout layout);
VkShaderStageFlagBits ToShaderType(ShaderStage type);
VkPipelineStageFlags2 ToPipelineStage(PipelineStage stage);
VkAccessFlags2 ToAccessFlags(MemoryAccess access);

}  // namespace lr::Graphics::VK