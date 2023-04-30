//
// Created on Saturday 22nd April 2023 by exdal
//

#pragma once

#include "Buffer.hh"
#include "CommandList.hh"
#include "InputLayout.hh"
#include "Pipeline.hh"
#include "Image.hh"

namespace lr::Graphics::VK
{
VkFormat ToFormat(ImageFormat format);
VkFormat ToFormat(VertexAttribType format);
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