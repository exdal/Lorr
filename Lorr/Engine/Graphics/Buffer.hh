// Created on Saturday April 22nd 2023 by exdal
// Last modified on Sunday June 25th 2023 by exdal

#pragma once

#include "APIAllocator.hh"

namespace lr::Graphics
{
enum class BufferUsage : u32
{
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    TransferSrc = 1 << 3,
    TransferDst = 1 << 4,
    Storage = 1 << 5,
    SamplerDescriptor = 1 << 6,
    ResourceDescriptor = 1 << 7,
};
EnumFlags(BufferUsage);

struct BufferDesc
{
    BufferUsage m_UsageFlags = BufferUsage::Vertex;
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;

    u32 m_Stride = 1;
    u64 m_DataSize = 0;
};

struct Buffer : APIObject<VK_OBJECT_TYPE_BUFFER>
{
    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;
    u64 m_AllocatorData = ~0;

    u32 m_DescriptorIndex = -1;
    
    u32 m_Stride = 1;
    u64 m_DataSize = 0;
    u64 m_DataOffset = 0;
    u64 m_DeviceAddress = 0;

    VkBuffer m_pHandle = nullptr;
};

}  // namespace lr::Graphics