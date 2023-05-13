#include "RenderPass.hh"

#include "RenderGraph.hh"

namespace lr::Graphics
{
RenderPassBuilder::RenderPassBuilder(RenderGraph *pGraph)
    : m_pContext(pGraph->m_pContext),
      m_pGraph(pGraph)
{
    ZoneScoped;

    u64 memorySize = m_pContext->m_MADescriptor.Allocator.m_Size;
    u64 memoryPerDescriptor = memorySize / (u64)DescriptorType::Count;

    BufferDesc bufferDesc = {
        .m_UsageFlags = BufferUsage::ResourceDescriptor | BufferUsage::TransferSrc,
        .m_TargetAllocator = ResourceAllocator::Descriptor,
        .m_DataLen = memoryPerDescriptor - 256,
    };

    for (auto &info : m_DescriptorBufferInfos)
        info.m_pBuffer = m_pContext->CreateBuffer(&bufferDesc);

    DescriptorLayoutElement pElements[] = {
        { 0, DescriptorType::Sampler, ShaderStage::All },
        { 1, DescriptorType::SampledImage, ShaderStage::All },
        { 2, DescriptorType::UniformBuffer, ShaderStage::All },
        // { 3, DescriptorType::StorageImage, ShaderStage::All },
        // { 4, DescriptorType::StorageBuffer, ShaderStage::All },
        { 5, DescriptorType::UniformBuffer, ShaderStage::All },
    };
    m_pDescriptorLayout = m_pContext->CreateDescriptorSetLayout(pElements);

    PushConstantDesc pushConstantDesc(ShaderStage::All, 0, 256);
    m_pPipelineLayout = m_pContext->CreatePipelineLayout(m_pDescriptorLayout, pushConstantDesc);
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

void RenderPassBuilder::SetDescriptorSet(DescriptorType type, eastl::span<DescriptorGetInfo> elements)
{
    ZoneScoped;

    auto &[pBuffer, mapOffset] = m_DescriptorBufferInfos[(u32)type];
    u64 typeSize = m_pContext->GetDescriptorSize(type);

    u64 dataSize = typeSize * elements.size();
    u32 bindingStart = mapOffset / typeSize;

    void *pSetData = nullptr;
    m_pContext->MapMemory(pBuffer, pSetData, mapOffset, dataSize);
    mapOffset += dataSize;

    for (DescriptorGetInfo &element : elements)
    {
        element.m_DescriptorIndex = bindingStart;
        u64 offset = m_pContext->GetDescriptorSetLayoutBindingOffset(m_pDescriptorLayout, bindingStart++);
        m_pContext->GetDescriptorData(type, element, typeSize, (u8 *)pSetData + offset);
    }

    m_pContext->UnmapMemory(pBuffer);
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