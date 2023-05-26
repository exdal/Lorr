// Created on Tuesday March 14th 2023 by exdal
// Last modified on Friday May 26th 2023 by exdal

#include "RenderPass.hh"

#include "Core/Config.hh"

#include "RenderGraph.hh"

#define LR_DESCRIPTOR_INDEX_SAMPLER 0
#define LR_DESCRIPTOR_INDEX_BUFFER 0
#define LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE 1
#define LR_DESCRIPTOR_INDEX_STORAGE_IMAGE 2

namespace lr::Graphics
{
constexpr u32 DescriptorTypeToResourceBinding(DescriptorType type)
{
    constexpr u32 kBindings[] = {
        LR_DESCRIPTOR_INDEX_BUFFER,         // Sampler
        LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE,  // SampledImage
        LR_DESCRIPTOR_INDEX_BUFFER,         // UniformBuffer
        LR_DESCRIPTOR_INDEX_STORAGE_IMAGE,  // StorageImage
        LR_DESCRIPTOR_INDEX_BUFFER,         // StorageBuffer
    };

    return kBindings[(u32)type];
}

constexpr DescriptorType ResourceBindingToDescriptorType(u32 binding)
{
    constexpr DescriptorType kBindings[] = {
        DescriptorType::StorageBuffer,  // LR_DESCRIPTOR_INDEX_BUFFER
        DescriptorType::SampledImage,   // LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE
        DescriptorType::StorageImage,   // LR_DESCRIPTOR_INDEX_STORAGE_IMAGE
    };

    return kBindings[binding];
}

RenderPassBuilder::RenderPassBuilder(RenderGraph *pGraph)
    : m_pContext(pGraph->m_pContext),
      m_pGraph(pGraph)
{
    ZoneScoped;

    m_DescriptorAllocator.Init({ .m_DataSize = CONFIG_GET_VAR(gpm_descriptor) });

    DescriptorLayoutElement pResourceLayoutElements[] = {
        { LR_DESCRIPTOR_INDEX_BUFFER, DescriptorType::StorageBuffer, ShaderStage::All },
        { LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE, DescriptorType::SampledImage, ShaderStage::All },
        { LR_DESCRIPTOR_INDEX_STORAGE_IMAGE, DescriptorType::SampledImage, ShaderStage::All },
    };
    DescriptorSetLayout *pResourceLayout = m_pContext->CreateDescriptorSetLayout(pResourceLayoutElements);

    DescriptorLayoutElement samplerLayoutElement(
        LR_DESCRIPTOR_INDEX_SAMPLER, DescriptorType::Sampler, ShaderStage::All);
    DescriptorSetLayout *pSamplerLayout = m_pContext->CreateDescriptorSetLayout(samplerLayoutElement);

    /// This shit is actually complex and debugging is ~~non-existent~~ pain, so here is the quick rundown:
    ///
    /// First we allocate small blocks of memory, which is responsible for holding descriptor data
    /// at each type. THEY ARE NOT THE RESOURCE ITSELF, THEY ARE DESCRIPTORS OF IT'S RESOURCE TYPE
    /// Also don't forget that descriptors are technically pointers so, these memory blocks are
    /// literally arrays of pointers, which point to the actual resource itself
    ///
    /// `LR_DESCRIPTOR_INDEX_DEVICE_ADDRESS_TABLE` buffer is special binding

    // clang-format off
    u64 size = m_pContext->AlignUpDescriptorOffset(1);  // TODO: Proper memory management...    
    InitDescriptorType(LR_DESCRIPTOR_INDEX_SAMPLER, BufferUsage::SamplerDescriptor, pSamplerLayout, size);
    
    InitDescriptorType(LR_DESCRIPTOR_INDEX_BUFFER, BufferUsage::ResourceDescriptor, pResourceLayout, size);
    InitDescriptorType(LR_DESCRIPTOR_INDEX_SAMPLED_IMAGE, BufferUsage::ResourceDescriptor, pResourceLayout, size);
    InitDescriptorType(LR_DESCRIPTOR_INDEX_STORAGE_IMAGE, BufferUsage::ResourceDescriptor, pResourceLayout, size);
    // clang-format on

    /// Now after we complete the allocation of blocks, we init our pipeline layout, this is just
    /// straight forward and obvious so no explanation

    DescriptorSetLayout *pLayouts[] = { pResourceLayout, pSamplerLayout };
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

    if (usage & BufferUsage::ResourceDescriptor)
        m_ResourceDescriptors.push_back(info);
    else
        m_SamplerDescriptor = info;
}

DescriptorBufferInfo *RenderPassBuilder::GetDescriptorBuffer(u32 binding)
{
    ZoneScoped;

    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
        if (info.m_BindingID == binding)
            return &info;

    return nullptr;
}

void RenderPassBuilder::BuildPass(RenderPass *pPass)
{
    ZoneScoped;

    m_pRenderPass = pPass;
    m_GraphicsPipelineInfo = {};
    m_ComputePipelineInfo = {};

    if (pPass->m_Flags & RenderPassFlag::AllowSetup)
    {
        if (pPass->m_PassType == CommandType::Graphics)
        {
            SetupGraphicsPass();
        }

        m_pRenderPass->m_ResourceIndex -= m_pRenderPass->m_ResourceCount;
    }
}

void RenderPassBuilder::SetupGraphicsPass()
{
    ZoneScoped;

    m_pGraphicsPass->Setup(m_pContext, this);

    if (!(m_pGraphicsPass->m_Flags & RenderPassFlag::SkipRendering))
    {
        m_GraphicsPipelineInfo.m_pLayout = m_pPipelineLayout;
        m_pGraphicsPass->m_pPipeline = m_pContext->CreateGraphicsPipeline(&m_GraphicsPipelineInfo);
    }
}

void RenderPassBuilder::SetColorAttachment(
    NameID resource, InputAttachmentOp attachmentOp, InputResourceFlag flags)
{
    ZoneScoped;

    m_pRenderPass->m_ResourceCount++;

    RenderPassInput attachment = {
        .m_Hash = Hash::FNV64String(resource),
        .m_Flags = flags | InputResourceFlag::ColorAttachment,
        .m_AttachmentOp = attachmentOp,
    };
    m_pGraph->m_InputResources.push_back(attachment);
    m_pRenderPass->m_ResourceIndex = m_pGraph->m_InputResources.size();  // we will deal with this at the end

    Image *pImageHandle = m_pGraph->GetInputImage(attachment);
    m_GraphicsPipelineInfo.m_ColorAttachmentFormats.push_back(pImageHandle->m_Format);
}

void RenderPassBuilder::SetInputResource(NameID resource, InputResourceAccess access, InputResourceFlag flags)
{
    ZoneScoped;

    m_pRenderPass->m_ResourceCount++;

    RenderPassInput attachment = {
        .m_Hash = Hash::FNV64String(resource),
        .m_Flags = flags,
        .m_Access = access,
    };
    m_pGraph->m_InputResources.push_back(attachment);
    m_pRenderPass->m_ResourceIndex = m_pGraph->m_InputResources.size();
}

void RenderPassBuilder::SetBlendAttachment(const ColorBlendAttachment &attachment)
{
    ZoneScoped;

    m_GraphicsPipelineInfo.m_BlendAttachments.push_back(attachment);
}

void RenderPassBuilder::SetBufferDescriptor(eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    DescriptorBufferInfo *pBufferInfo = GetDescriptorBuffer(LR_DESCRIPTOR_INDEX_BUFFER);

    u8 *pData = pBufferInfo->m_pData;
    u64 &offset = pBufferInfo->m_Offset;

    u64 typeSize = sizeof(u64);
    u64 dataSize = typeSize * elements.size();
    u32 descriptorIndex = offset / typeSize;

    u64 mapOffset = 0;

    for (DescriptorGetInfo &element : elements)
    {
        /// We don't use binding offset here because we simply don't need it. Resources are accessed
        /// by their indexes to their descriptor buffers. So instead of pushing into the binding offset
        /// we push into the buffer offset.

        memcpy((u8 *)pData + mapOffset, &element.m_BufferInfo.address, typeSize);

        mapOffset += typeSize;
        // element.m_DescriptorIndex = descriptorIndex++;
    }

    offset += dataSize;
}

void RenderPassBuilder::SetShader(Shader *pShader)
{
    ZoneScoped;

    m_GraphicsPipelineInfo.m_Shaders.push_back(pShader);
}

void RenderPassBuilder::SetInputLayout(const InputLayout &layout)
{
    ZoneScoped;

    m_GraphicsPipelineInfo.m_InputLayout = layout;
}

void RenderPassBuilder::WriteDescriptorAddress(eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    // DescriptorBufferInfo *pInfo = GetDescriptorBuffer(LR_DESCRIPTOR_INDEX_DEVICE_ADDRESS_TABLE);
    // if (!pInfo)
    //     return;

    // Buffer *pBuffer = pInfo->m_pBuffer;
    // u64 &offset = pInfo->m_BufferOffset;
    // u64 dataSize = elements.size() * pBuffer->m_Stride;

    // void *pMapData = nullptr;
    // m_pContext->MapMemory(pBuffer, pMapData, offset, dataSize);

    // u64 mapOffset = 0;
    // for (DescriptorGetInfo &element : elements)
    // {
    //     memcpy((u8 *)pMapData + mapOffset, &element.m_BufferInfo.address, pBuffer->m_Stride);
    // }

    // m_pContext->UnmapMemory(pBuffer);
    // offset += mapOffset;
}

u64 RenderPassBuilder::GetResourceBufferSize()
{
    ZoneScoped;

    u64 size = 0;
    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
        size += m_pContext->AlignUpDescriptorOffset(info.m_Offset);

    return size;
}

u64 RenderPassBuilder::GetSamplerBufferSize()
{
    ZoneScoped;

    DescriptorBufferInfo *pInfo = GetDescriptorBuffer(LR_DESCRIPTOR_INDEX_SAMPLER);
    if (!pInfo)
        return 0;

    return m_pContext->AlignUpDescriptorOffset(pInfo->m_Offset);
}

void RenderPassBuilder::GetResourceDescriptors(Buffer *pDst, CommandList *pList)
{
    ZoneScoped;

    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::BufferFrametime,
        .m_DataLen = GetResourceBufferSize(),
    };
    Buffer *pResourceDescriptor = m_pContext->CreateBuffer(&bufferDesc);

    void *pMapData = nullptr;
    m_pContext->MapMemory(pResourceDescriptor, pMapData, 0, bufferDesc.m_DataLen);

    for (DescriptorBufferInfo &info : m_ResourceDescriptors)
    {
        if (info.m_Offset == 0)
            continue;

        BufferDesc descriptorBufferDesc = {
            .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferSrc,
            .m_TargetAllocator = ResourceAllocator::Descriptor,
            .m_DataLen = info.m_Offset,
        };
        Buffer *pDescriptorBuffer = m_pContext->CreateBuffer(&descriptorBufferDesc);

        void *pDescriptorData = nullptr;
        m_pContext->MapMemory(pDescriptorBuffer, pDescriptorData, 0, descriptorBufferDesc.m_DataLen);
        memcpy(pDescriptorData, info.m_pData, info.m_Offset);
        m_pContext->UnmapMemory(pDescriptorBuffer);

        DescriptorType elementType = ResourceBindingToDescriptorType(info.m_BindingID);

        u64 bindingOffset = m_pContext->GetDescriptorSetLayoutBindingOffset(info.m_pLayout, info.m_BindingID);
        u64 bindingSize = m_pContext->GetDescriptorSize(elementType);

        DescriptorGetInfo bufferInfo(pDescriptorBuffer);
        m_pContext->GetDescriptorData(elementType, bufferInfo, bindingSize, (u8 *)pMapData + bindingOffset);
    }

    m_pContext->UnmapMemory(pResourceDescriptor);

    pList->CopyBuffer(pResourceDescriptor, pDst, 0, 0, bufferDesc.m_DataLen);
}

}  // namespace lr::Graphics