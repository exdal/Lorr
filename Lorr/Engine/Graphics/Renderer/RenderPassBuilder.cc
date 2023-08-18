// Created on Friday July 14th 2023 by exdal
// Last modified on Friday August 11th 2023 by exdal

#include "RenderPassBuilder.hh"

#include "Core/Config.hh"

#include "Graphics/Descriptor.hh"
#include "Graphics/Pipeline.hh"

// These indexes are SET indexes
#define LR_DESCRIPTOR_BDA_BUFFER 0
#define LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGE 2
#define LR_DESCRIPTOR_INDEX_SAMPLER 3

namespace lr::Graphics
{
constexpr u32 DescriptorTypeToSet(DescriptorType type)
{
    constexpr u32 kBindings[] = {
        LR_DESCRIPTOR_INDEX_SAMPLER,        // Sampler
        LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE,  // SampledImage
        LR_DESCRIPTOR_BDA_BUFFER,           // UniformBuffer
        LR_DESCRIPTOR_INDEX_STORAGE_IMAGE,  // StorageImage
        LR_DESCRIPTOR_BDA_BUFFER,           // StorageBuffer
    };

    return kBindings[(u32)type];
}

constexpr DescriptorType SetToDescriptorType(u32 binding)
{
    constexpr DescriptorType kBindings[] = {
        DescriptorType::StorageBuffer,  // LR_DESCRIPTOR_BDA_BUFFER
        DescriptorType::SampledImage,   // LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE
        DescriptorType::StorageImage,   // LR_DESCRIPTOR_INDEX_STORAGE_IMAGE
        DescriptorType::Sampler,        // LR_DESCRIPTOR_INDEX_SAMPLER
    };

    return kBindings[binding];
}

DescriptorBuilder::DescriptorBuilder(APIContext *pContext)
    : m_pContext(pContext)
{
    ZoneScoped;

    eastl::vector<DescriptorSetLayout *> layouts = {};
    auto InitDescriptorInfo = [&, this](u32 binding, DescriptorInfoType infoType)
    {
        DescriptorType type = SetToDescriptorType(binding);
        DescriptorLayoutElement layoutElement(0, type, ShaderStage::All);
        layouts.push_back(m_pContext->CreateDescriptorSetLayout(layoutElement));

        DescriptorInfo &info = m_DescriptorInfos.push_back();
        info.m_Type = infoType;
        info.m_SetID = binding;
        info.m_Allocator.Init({ .m_DataSize = m_pContext->GetDescriptorSize(type) * 256 });
    };

    InitDescriptorInfo(LR_DESCRIPTOR_BDA_BUFFER, DescriptorInfoType::BDA);
    InitDescriptorInfo(LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE, DescriptorInfoType::Resource);
    InitDescriptorInfo(LR_DESCRIPTOR_INDEX_STORAGE_IMAGE, DescriptorInfoType::Resource);
    InitDescriptorInfo(LR_DESCRIPTOR_INDEX_SAMPLER, DescriptorInfoType::Sampler);

    PushConstantDesc pushConstantDesc(ShaderStage::All, 0, 256);
    m_pPipelineLayout = m_pContext->CreatePipelineLayout(layouts, pushConstantDesc);

    for (DescriptorSetLayout *pLayout : layouts)
        m_pContext->DeleteDescriptorSetLayout(pLayout);
}

DescriptorBuilder::~DescriptorBuilder()
{
    ZoneScoped;
}

DescriptorBuilder::DescriptorInfo *DescriptorBuilder::GetDescriptorInfo(u32 set)
{
    ZoneScoped;

    for (DescriptorInfo &info : m_DescriptorInfos)
        if (info.m_SetID == set)
            return &info;

    return nullptr;
}

u64 DescriptorBuilder::GetDescriptorSize(DescriptorType type)
{
    ZoneScoped;

    // clang-format off
    return DescriptorTypeToSet(type) == LR_DESCRIPTOR_BDA_BUFFER 
            ? sizeof(u64) : m_pContext->GetDescriptorSize(type);
    // clang-format on
}

void DescriptorBuilder::SetDescriptors(eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    for (DescriptorGetInfo &descriptorInfo : elements)
    {
        u32 targetSet = DescriptorTypeToSet(descriptorInfo.m_Type);
        DescriptorInfo *pDescriptorInfo = GetDescriptorInfo(targetSet);
        u64 descriptorSize = GetDescriptorSize(descriptorInfo.m_Type);
        u64 allocatorOffset = pDescriptorInfo->m_Allocator.Size();
        u8 *pData = pDescriptorInfo->m_Allocator.Allocate(descriptorSize);
        u32 descriptorIndex = allocatorOffset / descriptorSize;

        if (pDescriptorInfo->m_Type == DescriptorInfoType::BDA)
        {
            memcpy(pData, &descriptorInfo.m_pBuffer->m_DeviceAddress, descriptorSize);
            descriptorInfo.m_pBuffer->m_DescriptorIndex = descriptorIndex;
        }
        else
        {
            m_pContext->GetDescriptorData(descriptorInfo, descriptorSize, pData, descriptorIndex);
        }
    }
}

u64 DescriptorBuilder::GetBufferSize(DescriptorInfoType type)
{
    ZoneScoped;

    u64 size = 0;
    for (DescriptorInfo &info : m_DescriptorInfos)
    {
        if (info.m_Type != type)
            continue;

        if (info.m_Type == DescriptorInfoType::BDA)
            size += m_pContext->GetDescriptorSizeAligned(DescriptorType::StorageBuffer);
        else
            size += m_pContext->AlignUpDescriptorOffset(info.m_Allocator.Size());
    }

    return size;
}

void DescriptorBuilder::GetDescriptorOffsets(DescriptorInfoType type, u64 *pOffsetsOut, u32 *pDescriptorSizeOut)
{
    ZoneScoped;

    u64 descriptorOffset = 0;
    u32 descriptorCount = 0;
    for (DescriptorInfo &info : m_DescriptorInfos)
    {
        if (info.m_Type != type)
            continue;

        *pDescriptorSizeOut = ++descriptorCount;
        if (pOffsetsOut)
            *(pOffsetsOut++) = descriptorOffset;

        descriptorOffset += m_pContext->AlignUpDescriptorOffset(info.m_Allocator.Size());
    }
}

}  // namespace lr::Graphics