//
// Created on Saturday 22nd April 2023 by exdal
//

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
    u64 m_DataLen = 0;
};

struct Buffer : APIObject<VK_OBJECT_TYPE_BUFFER>
{
    void Init(BufferDesc *pDesc)
    {
        m_TargetAllocator = pDesc->m_TargetAllocator;
        m_Stride = pDesc->m_Stride;
        m_DataLen = pDesc->m_DataLen;
    }

    ResourceAllocator m_TargetAllocator = ResourceAllocator::None;
    void *m_pAllocatorData = nullptr;

    u32 m_Stride = 1;
    u64 m_DataLen = 0;

    u64 m_AllocatorOffset = 0;
    u64 m_DeviceDataLen = 0;
    u64 m_DeviceAddress = 0;

    VkBuffer m_pHandle = nullptr;
    VkBufferView m_pViewHandle = nullptr;
    VkDeviceMemory m_pMemoryHandle = nullptr;
};

}  // namespace lr::Graphics