// Created on Friday July 14th 2023 by exdal
// Last modified on Sunday July 16th 2023 by exdal

#include "RenderPassBuilder.hh"

#include "Core/Config.hh"

#include "Graphics/Descriptor.hh"
#include "Graphics/Pipeline.hh"

// These indexes are SET indexes
#define LR_DESCRIPTOR_BDA_LUT_BUFFER 0
#define LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGE 2
#define LR_DESCRIPTOR_INDEX_SAMPLER 3

namespace lr::Graphics
{
constexpr u32 DescriptorTypeToBinding(DescriptorType type)
{
    constexpr u32 kBindings[] = {
        LR_DESCRIPTOR_INDEX_SAMPLER,        // Sampler
        LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE,  // SampledImage
        LR_DESCRIPTOR_BDA_LUT_BUFFER,       // UniformBuffer
        LR_DESCRIPTOR_INDEX_STORAGE_IMAGE,  // StorageImage
        LR_DESCRIPTOR_BDA_LUT_BUFFER,       // StorageBuffer
    };

    return kBindings[(u32)type];
}

constexpr DescriptorType BindingToDescriptorType(u32 binding)
{
    constexpr DescriptorType kBindings[] = {
        DescriptorType::StorageBuffer,  // LR_DESCRIPTOR_BDA_LUT_BUFFER
        DescriptorType::SampledImage,   // LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE
        DescriptorType::StorageImage,   // LR_DESCRIPTOR_INDEX_STORAGE_IMAGE
        DescriptorType::Sampler,        // LR_DESCRIPTOR_INDEX_SAMPLER
    };

    return kBindings[binding];
}

RenderPassBuilder::RenderPassBuilder(APIContext *pContext)
    : m_pContext(pContext)
{
    ZoneScoped;

    m_DescriptorAllocator.Init({ .m_DataSize = CONFIG_GET_VAR(gpm_descriptor) });

    DescriptorLayoutElement bufferDescriptorElement(0, DescriptorType::StorageBuffer, ShaderStage::All);
    DescriptorLayoutElement imageDescriptorElement(0, DescriptorType::SampledImage, ShaderStage::All);
    DescriptorLayoutElement storageImageDescriptorElement(0, DescriptorType::StorageImage, ShaderStage::All);

    DescriptorSetLayout *pBufferLayout = m_pContext->CreateDescriptorSetLayout(bufferDescriptorElement);
    DescriptorSetLayout *pImageLayout = m_pContext->CreateDescriptorSetLayout(imageDescriptorElement);
    DescriptorSetLayout *pStorageImageLayout =
        m_pContext->CreateDescriptorSetLayout(storageImageDescriptorElement);

    DescriptorLayoutElement samplerLayoutElement(0, DescriptorType::Sampler, ShaderStage::All);
    DescriptorSetLayout *pSamplerLayout = m_pContext->CreateDescriptorSetLayout(samplerLayoutElement);

    /// This shit is actually complex and debugging is ~~non-existent~~ pain, so here is the quick rundown:
    ///
    /// First we allocate small blocks of memory, which is responsible for holding descriptor data
    /// at each type. THEY ARE NOT THE RESOURCE ITSELF, THEY ARE DESCRIPTORS OF IT'S RESOURCE TYPE
    /// Also don't forget that descriptors are technically pointers so, these memory blocks are
    /// literally arrays of pointers, which point to the actual resource itself

    // clang-format off
    u64 size = m_pContext->AlignUpDescriptorOffset(1);  // TODO: Proper memory management...    
    InitDescriptorType(LR_DESCRIPTOR_BDA_LUT_BUFFER, BufferUsage::ResourceDescriptor, pBufferLayout, size);
    InitDescriptorType(LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE, BufferUsage::ResourceDescriptor, pImageLayout, size);
    InitDescriptorType(LR_DESCRIPTOR_INDEX_STORAGE_IMAGE, BufferUsage::ResourceDescriptor, pStorageImageLayout, size);
    
    InitDescriptorType(LR_DESCRIPTOR_INDEX_SAMPLER, BufferUsage::SamplerDescriptor, pSamplerLayout, size);
    // clang-format on

    /// Now after we complete the allocation of blocks, we init our pipeline layout, this is just
    /// straight forward and obvious so no explanation

    DescriptorSetLayout *pLayouts[] = { pBufferLayout, pImageLayout, pStorageImageLayout, pSamplerLayout };
    PushConstantDesc pushConstantDesc(ShaderStage::All, 0, 256);
    m_pPipelineLayout = m_pContext->CreatePipelineLayout(pLayouts, pushConstantDesc);

    /// So how are resources going to be accessed? It's simple, descriptor indexing!
    /// We can just map the descriptor data into the descriptor buffer, its THAT simple.
    /// Is this safe? FUCK NO, but it's funny when my gpu crashes and i have a reason to
    /// get up and walk a bit
    ///
    /// See `SetDescriptor` for more info.
}

RenderPassBuilder::~RenderPassBuilder()
{
    ZoneScoped;
}

void RenderPassBuilder::InitDescriptorType(
    u32 binding, BufferUsage usage, DescriptorSetLayout *pLayout, u64 memSize)
{
    ZoneScoped;

    DescriptorBufferInfo info = {
        .m_pLayout = pLayout,
        .m_BindingID = binding,
    };

    info.m_pData = (u8 *)m_DescriptorAllocator.Allocate(memSize);
    info.m_DataSize = memSize;

    if (usage & BufferUsage::ResourceDescriptor)
        m_ResourceDescriptors.push_back(info);
    else
        m_SamplerDescriptor = info;
}

DescriptorBufferInfo *RenderPassBuilder::GetDescriptorBuffer(u32 binding)
{
    ZoneScoped;

    if (binding == LR_DESCRIPTOR_INDEX_SAMPLER)
        return &m_SamplerDescriptor;

    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
        if (info.m_BindingID == binding)
            return &info;

    return nullptr;
}

void RenderPassBuilder::SetDescriptor(DescriptorType type, eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    u32 targetSet = DescriptorTypeToBinding(type);
    DescriptorBufferInfo *pDescriptorInfo = GetDescriptorBuffer(targetSet);

    u8 *pData = pDescriptorInfo->m_pData;
    u64 &offset = pDescriptorInfo->m_Offset;

    u64 typeSize =
        targetSet == LR_DESCRIPTOR_BDA_LUT_BUFFER ? sizeof(u64) : m_pContext->GetDescriptorSize(type);
    u32 descriptorIndex = offset / typeSize;

    for (DescriptorGetInfo &element : elements)
    {
        /// We don't use binding offset here because we simply don't need it. Resources are accessed
        /// by their indexes to their descriptor buffers. So instead of pushing into the binding offset
        /// we push into the buffer offset.

        if (targetSet != LR_DESCRIPTOR_BDA_LUT_BUFFER)
        {
            DescriptorGetInfo descriptorInfo(element.m_pImage);
            m_pContext->GetDescriptorData(type, descriptorInfo, typeSize, pData + offset);
        }
        else
        {
            memcpy(pData + offset, &element.m_pBuffer->m_DeviceAddress, typeSize);
        }

        offset += typeSize;
        element.m_DescriptorIndex = descriptorIndex++;
    }
}

u64 RenderPassBuilder::GetResourceBufferSize()
{
    ZoneScoped;

    u64 size = 0;
    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
        if (info.m_BindingID == LR_DESCRIPTOR_BDA_LUT_BUFFER)
            size += m_pContext->AlignUpDescriptorOffset(
                m_pContext->GetDescriptorSize(BindingToDescriptorType(LR_DESCRIPTOR_BDA_LUT_BUFFER)));
        else
            size += m_pContext->AlignUpDescriptorOffset(info.m_DataSize);

    return size;
}

u64 RenderPassBuilder::GetSamplerBufferSize()
{
    ZoneScoped;

    return m_pContext->AlignUpDescriptorOffset(m_SamplerDescriptor.m_DataSize);
}

void RenderPassBuilder::GetResourceDescriptorOffsets(u64 *pOffsetsOut, u32 *pDescriptorSizeOut)
{
    ZoneScoped;

    u64 descriptorOffset = 0;
    u32 descriptorCount = 0;
    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
    {
        *pDescriptorSizeOut = ++descriptorCount;
        if (pOffsetsOut)
            *(pOffsetsOut++) = descriptorOffset;

        descriptorOffset += m_pContext->AlignUpDescriptorOffset(info.m_DataSize);
    }
}

Buffer *RenderPassBuilder::CreateResourceDescriptorBuffer()
{
    ZoneScoped;

    BufferDesc resourceDescriptorBufferDesc = {
        .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::Descriptor,
        .m_DataSize = GetResourceBufferSize(),
    };
    Buffer *pResourceDescriptor = m_pContext->CreateBuffer(&resourceDescriptorBufferDesc);
    m_pContext->SetObjectName(pResourceDescriptor, "ResourceDescriptor");

    u64 mapOffset = 0;
    void *pMapData = nullptr;
    m_pContext->MapMemory(ResourceAllocator::Descriptor, pMapData, 0, ~0);

    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
    {
        if (info.m_BindingID == LR_DESCRIPTOR_BDA_LUT_BUFFER)
        {
            BufferDesc bdaBufferDesc = {
                .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::Storage,
                .m_TargetAllocator = ResourceAllocator::Descriptor,
                .m_DataSize = info.m_DataSize,
            };
            Buffer *pBDABuffer = m_pContext->CreateBuffer(&bdaBufferDesc);
            m_pContext->SetObjectName(pBDABuffer, "DescriptorBuffer-BDALUT");

            DescriptorGetInfo bdaBufferDescriptor(pBDABuffer);
            DescriptorType elementType = BindingToDescriptorType(info.m_BindingID);
            u64 bindingSize = m_pContext->GetDescriptorSize(elementType);

            m_pContext->GetDescriptorData(
                elementType, bdaBufferDescriptor, bindingSize, (u8 *)pMapData + mapOffset);
            memcpy((u8 *)pMapData + pBDABuffer->m_DataOffset, info.m_pData, info.m_Offset);
        }
        else
        {
            /// Non-buffer descriptors are different, we already get their descriptors unlike buffers
            /// so it's safe to just copy their memory into resource descriptor

            memcpy((u8 *)pMapData + mapOffset, info.m_pData, info.m_DataSize);
        }

        mapOffset += info.m_DataSize;
    }

    m_pContext->UnmapMemory(ResourceAllocator::Descriptor);

    return pResourceDescriptor;
}

Buffer *RenderPassBuilder::CreateSamplerDescriptorBuffer()
{
    ZoneScoped;

    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::SamplerDescriptor | BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::Descriptor,
        .m_DataSize = GetSamplerBufferSize(),
    };
    Buffer *pSamplerDescriptor = m_pContext->CreateBuffer(&bufferDesc);

    u64 mapOffset = 0;
    void *pMapData = nullptr;
    m_pContext->MapBuffer(pSamplerDescriptor, pMapData, 0, bufferDesc.m_DataSize);

    memcpy((u8 *)pMapData, m_SamplerDescriptor.m_pData, m_SamplerDescriptor.m_Offset);

    m_pContext->UnmapBuffer(pSamplerDescriptor);

    return pSamplerDescriptor;
}

}  // namespace lr::Graphics