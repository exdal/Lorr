// Created on Friday July 14th 2023 by exdal
// Last modified on Friday August 11th 2023 by exdal

#include "RenderPassBuilder.hh"

#include "Core/Config.hh"

#include "Graphics/APIResource.hh"
#include "Graphics/Pipeline.hh"

namespace lr::Graphics
{
constexpr DescriptorSetIndex DescriptorTypeToSet(DescriptorType type)
{
    constexpr DescriptorSetIndex kBindings[] = {
        DescriptorSetIndex::Sampler,       // Sampler
        DescriptorSetIndex::Image,         // SampledImage
        DescriptorSetIndex::Buffer,        // UniformBuffer
        DescriptorSetIndex::StorageImage,  // StorageImage
        DescriptorSetIndex::Buffer         // StorageBuffer
    };

    return kBindings[(u32)type];
}

constexpr DescriptorType SetToDescriptorType(DescriptorSetIndex set)
{
    constexpr DescriptorType kBindings[] = {
        DescriptorType::StorageBuffer,  // DescriptorSetIndex::Buffer
        DescriptorType::SampledImage,   // DescriptorSetIndex::Image
        DescriptorType::StorageImage,   // DescriptorSetIndex::StorageImage
        DescriptorType::Sampler         // DescriptorSetIndex::Sampler
    };

    return kBindings[(u32)set];
}

DescriptorBuilder::DescriptorBuilder(APIContext *pContext)
    : m_pContext(pContext)
{
    ZoneScoped;

    eastl::vector<DescriptorSetLayout *> layouts = {};
    auto InitDescriptorInfo =
        [&, this](DescriptorSetIndex set, DescriptorInfoType infoType)
    {
        DescriptorType type = SetToDescriptorType(set);
        DescriptorLayoutElement layoutElement(0, type, ShaderStage::All);
        layouts.push_back(m_pContext->CreateDescriptorSetLayout(layoutElement));

        DescriptorInfo &info = m_DescriptorInfos.push_back();
        info.m_Type = infoType;
        info.m_Set = set;
        info.m_Allocator.Init(
            { .m_DataSize = m_pContext->GetDescriptorSize(type) * 256 });
    };

    InitDescriptorInfo(DescriptorSetIndex::Buffer, DescriptorInfoType::BDA);
    InitDescriptorInfo(DescriptorSetIndex::Image, DescriptorInfoType::Resource);
    InitDescriptorInfo(DescriptorSetIndex::StorageImage, DescriptorInfoType::Resource);
    InitDescriptorInfo(DescriptorSetIndex::Sampler, DescriptorInfoType::Sampler);

    PushConstantDesc pushConstantDesc(ShaderStage::All, 0, 256);
    m_pPipelineLayout = m_pContext->CreatePipelineLayout(layouts, pushConstantDesc);

    for (DescriptorSetLayout *pLayout : layouts)
        m_pContext->DeleteDescriptorSetLayout(pLayout);
}

DescriptorBuilder::~DescriptorBuilder()
{
    ZoneScoped;
}

DescriptorBuilder::DescriptorInfo *DescriptorBuilder::GetDescriptorInfo(
    DescriptorSetIndex set)
{
    ZoneScoped;

    for (DescriptorInfo &info : m_DescriptorInfos)
        if (info.m_Set == set)
            return &info;

    return nullptr;
}

u64 DescriptorBuilder::GetDescriptorSize(DescriptorType type)
{
    ZoneScoped;

    // clang-format off
    return DescriptorTypeToSet(type) == DescriptorSetIndex::Buffer 
            ? sizeof(u64) : m_pContext->GetDescriptorSize(type);
    // clang-format on
}

void DescriptorBuilder::SetDescriptors(eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    for (DescriptorGetInfo &descriptorInfo : elements)
    {
        DescriptorSetIndex targetSet = DescriptorTypeToSet(descriptorInfo.m_Type);
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
            m_pContext->GetDescriptorData(
                descriptorInfo, descriptorSize, pData, descriptorIndex);
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

void DescriptorBuilder::GetDescriptorOffsets(
    DescriptorInfoType type, u64 *pOffsetsOut, u32 *pDescriptorSizeOut)
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