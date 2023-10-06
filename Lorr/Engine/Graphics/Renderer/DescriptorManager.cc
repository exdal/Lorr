// Created on Friday July 14th 2023 by exdal
// Last modified on Friday August 11th 2023 by exdal

#include "DescriptorManager.hh"

#include "Graphics/APIResource.hh"
#include "Graphics/Pipeline.hh"

namespace lr::Graphics
{
constexpr DescriptorPoolType MapDescriptorType(DescriptorType type)
{
    constexpr DescriptorPoolType kTypes[] = {
        [(u32)DescriptorType::UniformBuffer] = DescriptorPoolType::BDA,
        [(u32)DescriptorType::StorageBuffer] = DescriptorPoolType::BDA,
        [(u32)DescriptorType::SampledImage] = DescriptorPoolType::Resource,
        [(u32)DescriptorType::StorageImage] = DescriptorPoolType::Resource,
        [(u32)DescriptorType::Sampler] = DescriptorPoolType::Sampler,
    };

    return kTypes[(u32)type];
}

constexpr BufferUsage MapBufferUsage(DescriptorPoolType type)
{
    constexpr BufferUsage kTypes[] = {
        [(u32)DescriptorPoolType::BDA] = BufferUsage::Storage,
        [(u32)DescriptorPoolType::Resource] = BufferUsage::ResourceDescriptor,
        [(u32)DescriptorPoolType::Sampler] = BufferUsage::SamplerDescriptor,
    };

    return kTypes[(u32)type];
}

void DescriptorManager::Init(DescriptorManagerDesc *pDesc)
{
    ZoneScoped;

    m_pContext = pDesc->m_pAPIContext;
    eastl::vector<DescriptorSetLayout *> layouts = {};
    eastl::array<usize, (u32)DescriptorPoolType::Count> bufferSizes = { 0 };

    // This is to add BDA address header into Resource Buffer
    // We only have one BDA Buffer so it's probably okay to hardcode this
    InitBDAPool();
    bufferSizes[(u32)DescriptorPoolType::Resource] +=
        m_pContext->GetDescriptorSizeAligned(DescriptorType::StorageBuffer);

    for (DescriptorPoolDesc &poolDesc : pDesc->m_Pools)
    {
        u64 descriptorSize = m_pContext->GetDescriptorSize(poolDesc.m_Type);
        usize &offset = bufferSizes[(u32)MapDescriptorType(poolDesc.m_Type)];

        // Get adjusted descriptor size
        u64 sizeBytes = descriptorSize * poolDesc.m_MaxDescriptors;
        sizeBytes = m_pContext->AlignUpDescriptorOffset(sizeBytes);

        DescriptorPool &pool = m_Pools.push_back();
        pool.m_Type = poolDesc.m_Type;
        pool.m_MaxDescriptors = poolDesc.m_MaxDescriptors;
        pool.m_Offset = offset;

        offset += sizeBytes;

        DescriptorLayoutElement layoutElement(0, poolDesc.m_Type, ShaderStage::All);
        layouts.push_back(m_pContext->CreateDescriptorSetLayout(layoutElement));
    }

    for (u32 i = 0; i < (u32)DescriptorPoolType::Count; i++)
    {
        BufferDesc bufferDesc = {
            .m_UsageFlags = MapBufferUsage((DescriptorPoolType)i),
            .m_TargetAllocator = ResourceAllocator::Descriptor,
            .m_DataSize = bufferSizes[i],
        };

        m_Buffers[i] = m_pContext->CreateBuffer(&bufferDesc);
    }

    PushConstantDesc pushConstantDesc(ShaderStage::All, 0, 256);
    m_pPipelineLayout = m_pContext->CreatePipelineLayout(layouts, pushConstantDesc);

    for (DescriptorSetLayout *pLayout : layouts)
        m_pContext->DeleteDescriptorSetLayout(pLayout);
}

void DescriptorManager::InitBDAPool(usize count)
{
    ZoneScoped;

    DescriptorPool &pool = m_Pools.push_back();
    pool.m_Type = DescriptorType::StorageBuffer;
    pool.m_MaxDescriptors = count;
    pool.m_Offset = 0;
}

void DescriptorManager::SetDescriptors(eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;


}

Buffer *DescriptorManager::GetBuffer(DescriptorPoolType type)
{
    ZoneScoped;

    return m_Buffers[(u32)type];
}

}  // namespace lr::Graphics