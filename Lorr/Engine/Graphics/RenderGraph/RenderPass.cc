#include "RenderPass.hh"

#include "STL/Vector.hh"

#include "RenderGraph.hh"

namespace lr::Graphics
{
RenderPassBuilder::RenderPassBuilder(RenderGraph *pGraph)
    : m_pContext(pGraph->m_pContext),
      m_pGraph(pGraph)
{
    ZoneScoped;

    // We will create a buffer that covers entire allocator, and continuously map/unmap it.
    // Then we will upload it to device visible memory block with staging buffers.

    u64 memorySize = m_pContext->m_MADescriptor.Allocator.m_Size;
    u64 samplerMemorySize = memorySize / 4;
    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::Descriptor,
        .m_DataLen = memorySize - samplerMemorySize,
    };
    m_ResourceDescriptorInfo.m_pBuffer = m_pContext->CreateBuffer(&bufferDesc);

    bufferDesc.m_DataLen = samplerMemorySize;
    m_SamplerDescriptorInfo.m_pBuffer = m_pContext->CreateBuffer(&bufferDesc);
}

RenderPassBuilder::~RenderPassBuilder()
{
    ZoneScoped;
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
        m_pGraphicsPass->m_pPipeline = m_pContext->CreateGraphicsPipeline(&m_GraphicsPipelineInfo);
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

void RenderPassBuilder::SetPushConstant(const PushConstantDesc &pushConstant)
{
    ZoneScoped;

    m_GraphicsPipelineInfo.m_PushConstants.push_back(pushConstant);
}

void RenderPassBuilder::SetResourceDescriptorSet(
    DescriptorSetLayout *pLayout, eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    Buffer *&pBuffer = m_ResourceDescriptorInfo.m_pBuffer;
    u64 &mapOffset = m_ResourceDescriptorInfo.m_BufferOffset;

    void *pSetData = nullptr;
    u64 setSize = m_pContext->GetDescriptorSetLayoutSize(pLayout);

    m_pContext->MapMemory(pBuffer, pSetData, mapOffset, setSize);
    mapOffset += setSize;

    for (DescriptorGetInfo &element : elements)
    {
        u64 offset = m_pContext->GetDescriptorSetLayoutBindingOffset(pLayout, element.m_Binding);
        u64 size = m_pContext->GetDescriptorSize(element.m_Type);
        m_pContext->GetDescriptorData(element, size, (u8 *)pSetData + offset);
    }

    m_pContext->UnmapMemory(pBuffer);
}

void RenderPassBuilder::SetSamplerDescriptorSet(
    DescriptorSetLayout *pLayout, eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    Buffer *&pBuffer = m_SamplerDescriptorInfo.m_pBuffer;
    u64 &mapOffset = m_SamplerDescriptorInfo.m_BufferOffset;

    void *pSetData = nullptr;
    u64 setSize = m_pContext->GetDescriptorSetLayoutSize(pLayout);

    m_pContext->MapMemory(pBuffer, pSetData, mapOffset, setSize);
    mapOffset += setSize;

    for (DescriptorGetInfo &element : elements)
    {
        u64 offset = m_pContext->GetDescriptorSetLayoutBindingOffset(pLayout, element.m_Binding);
        u64 size = m_pContext->GetDescriptorSize(DescriptorType::Sampler);
        m_pContext->GetDescriptorData(element, size, (u8 *)pSetData + offset);
    }

    m_pContext->UnmapMemory(pBuffer);
}

void RenderPassBuilder::SetDescriptorSetLayout(DescriptorSetLayout *pLayout)
{
    ZoneScoped;

    m_GraphicsPipelineInfo.m_Layouts.push_back(pLayout);
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

}  // namespace lr::Graphics