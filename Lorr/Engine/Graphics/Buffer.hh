// Created on Saturday April 22nd 2023 by exdal
// Last modified on Monday August 28th 2023 by exdal

#pragma once

#include "APIAllocator.hh"
#include "Common.hh"

namespace lr::Graphics
{
struct BufferDesc
{
    BufferUsage m_UsageFlags = BufferUsage::Vertex;
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
};

struct Buffer : APIObject<VK_OBJECT_TYPE_BUFFER>
{
    ResourceAllocator m_Allocator = ResourceAllocator::None;
    u64 m_AllocatorData = ~0;

    u32 m_DescriptorIndex = -1;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
    u64 m_DataOffset = 0;
    u64 m_DeviceAddress = 0;

    VkBuffer m_pHandle = nullptr;
};

}  // namespace lr::Graphics